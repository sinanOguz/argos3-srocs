/**
 * @file <argos3/plugins/robots/drone/hardware/drone.cpp>
 *
 * @author Michael Allwright - <allsey87@gmail.com>
 */

#include "drone.h"

#include <iio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <list>
#include <thread>

#include <argos3/core/control_interface/ci_controller.h>
#include <argos3/core/utility/logging/argos_log.h>
#include <argos3/core/utility/plugins/factory.h>
#include <argos3/core/utility/rate.h>
#include <argos3/core/wrappers/lua/lua_controller.h>

#include <argos3/plugins/robots/generic/hardware/sensor.h>
#include <argos3/plugins/robots/generic/hardware/actuator.h>

#define ADD_TRIGGER_PATH "/sys/bus/iio/devices/iio_sysfs_trigger/add_trigger"
#define REMOVE_TRIGGER_PATH "/sys/bus/iio/devices/iio_sysfs_trigger/remove_trigger"

#define SENSOR_TRIGGER_IDX 0

namespace argos {

   void CDrone::Init(TConfigurationNode& t_controller,
                     const std::string& str_controller_id,
                     const std::string& str_router_addr,
                     const std::string& str_pixhawk_conf,
                     UInt32 un_ticks_per_sec,
                     UInt32 un_length) {
      m_unTicksPerSec = un_ticks_per_sec;
      m_unLength = un_length;
      try {        
         /* Create an instance of the controller controller */
         CCI_Controller* pcController = CFactory<CCI_Controller>::New(t_controller.Value());
         m_pcController = dynamic_cast<CLuaController*>(pcController);
         if(m_pcController == nullptr) {
            THROW_ARGOSEXCEPTION("ERROR: controller \"" << t_controller.Value() << "\" is not a Lua controller");
         }
         /* set the controller ID */
         m_pcController->SetId(str_controller_id);
         /* connect to the router if address was specified */
         if(!str_router_addr.empty()) {
            size_t unHostnamePortPos = str_router_addr.find_last_of(':');
            try {
               if(unHostnamePortPos == std::string::npos) {
                  THROW_ARGOSEXCEPTION("The address of the router must be provided as \"hostname:port\"");
               }
               SInt32 nPort = std::stoi(str_router_addr.substr(unHostnamePortPos + 1), nullptr, 0);
               m_cSocket.Connect(str_router_addr.substr(0, unHostnamePortPos), nPort);
            }
            catch(CARGoSException& ex) {
               THROW_ARGOSEXCEPTION_NESTED("Could not connect to router", ex);
            }
         }
         /* parse and open the Pixhawk device if configuration was provided */
         if(!str_pixhawk_conf.empty()) {
            size_t unDevnameBaudPos = str_pixhawk_conf.find_last_of(':');
            try {
               if(unDevnameBaudPos == std::string::npos) {
                  THROW_ARGOSEXCEPTION("The pixhawk configuration must be provided as \"device:baudrate\"");
               }
               SInt32 nBaud = std::stoi(str_pixhawk_conf.substr(unDevnameBaudPos + 1), nullptr, 0);
               m_cPixhawk.Open(str_pixhawk_conf.substr(0, unDevnameBaudPos), nBaud);
            }
            catch(CARGoSException& ex) {
               THROW_ARGOSEXCEPTION_NESTED("Could not open Pixhawk", ex);
            }
         }
         /* Create the triggers */
         std::ofstream cAddTrigger;
         cAddTrigger.open(ADD_TRIGGER_PATH);
         cAddTrigger << std::to_string(SENSOR_TRIGGER_IDX) << std::flush;
         cAddTrigger.close();
         /* Create a local context for the IIO library */
         m_psContext = iio_create_local_context();
         /* validate the sensor update trigger */
         /*
         std::string strSensorUpdateTrigger("sysfstrig" + std::to_string(SENSOR_TRIGGER_IDX));
         m_psSensorUpdateTrigger = 
            ::iio_context_find_device(m_psContext, strSensorUpdateTrigger.c_str());
         if(m_psSensorUpdateTrigger == nullptr) { 
            THROW_ARGOSEXCEPTION("Could not find IIO trigger \"" << strSensorUpdateTrigger << "\"");
         }
         if(!::iio_device_is_trigger(m_psSensorUpdateTrigger)) {
            THROW_ARGOSEXCEPTION("IIO device \"" << strSensorUpdateTrigger << "\" is not a trigger");
         }
         */
         /* go through the actuators */
         std::string strImpl;
         /* Go through actuators */
         TConfigurationNode& tActuators = GetNode(t_controller, "actuators");
         TConfigurationNodeIterator itAct;
         for(itAct = itAct.begin(&tActuators);
             itAct != itAct.end();
             ++itAct) {
            /* itAct->Value() is the name of the current actuator */
            GetNodeAttribute(*itAct, "implementation", strImpl);
            CPhysicalActuator* pcAct = CFactory<CPhysicalActuator>::New(itAct->Value() + " (" + strImpl + ")");
            CCI_Actuator* pcCIAct = dynamic_cast<CCI_Actuator*>(pcAct);
            if(pcCIAct == nullptr) {
               THROW_ARGOSEXCEPTION("BUG: actuator \"" << itAct->Value() << "\" does not inherit from CCI_Actuator");
            }
            pcAct->SetRobot(*this);
            pcCIAct->Init(*itAct);
            m_vecActuators.emplace_back(pcAct);
            m_pcController->AddActuator(itAct->Value(), pcCIAct);
         }
         /* Go through sensors */
         TConfigurationNode& tSensors = GetNode(t_controller, "sensors");
         TConfigurationNodeIterator itSens;
         for(itSens = itSens.begin(&tSensors);
             itSens != itSens.end();
             ++itSens) {
            /* itSens->Value() is the name of the current actuator */
            GetNodeAttribute(*itSens, "implementation", strImpl);
            CPhysicalSensor* pcSens = CFactory<CPhysicalSensor>::New(itSens->Value() + " (" + strImpl + ")");
            CCI_Sensor* pcCISens = dynamic_cast<CCI_Sensor*>(pcSens);
            if(pcCISens == nullptr) {
               THROW_ARGOSEXCEPTION("BUG: sensor \"" << itSens->Value() << "\" does not inherit from CCI_Sensor");
            }
            pcSens->SetRobot(*this);
            pcCISens->Init(*itSens);
            m_vecSensors.emplace_back(pcSens);
            m_pcController->AddSensor(itSens->Value(), pcCISens);
         }        
         /* Set the controller id */
         char pchBuffer[64];
         if (::gethostname(pchBuffer, 64) == 0) {
            LOG << "[INFO] Setting controller id to hostname \""
                << pchBuffer << "\""
                << std::endl;
            m_pcController->SetId(pchBuffer);
         } 
         else {
            LOGERR << "[WARNING] Failed to get the hostname."
                   << "Setting controller id to \"drone\""
                   << std::endl;
            m_pcController->SetId("drone");
         }
         /* If the parameters node doesn't exist, create one */
         if(!NodeExists(t_controller, "params")) {
            TConfigurationNode tParamsNode("params");
            AddChildNode(t_controller, tParamsNode);
         }
         /* Init the controller with the parameters */
         m_pcController->Init(GetNode(t_controller, "params"));
         /* check for errors */
         if(!m_pcController->IsOK()) {
            THROW_ARGOSEXCEPTION("Controller: " << m_pcController->GetErrorMessage());
         }
      }
      catch(CARGoSException& ex) {
         THROW_ARGOSEXCEPTION_NESTED("Failed to initialize drone", ex);
      }
   }

   /****************************************/
   /****************************************/

   void CDrone::Execute() {
      /* initialize the tick rate */
      CRate cRate(m_unTicksPerSec);
      /* start the main control loop */
      try {
         for(UInt32 unControllerTick = 0;
             !m_unLength || unControllerTick < m_unLength;
             ++unControllerTick) {
            /* request samples from the sensors */
            //::iio_device_attr_write_bool(m_psSensorUpdateTrigger, "trigger_now", true);
            /* update the sensors on this thread */
            for(CPhysicalSensor* pc_sensor : m_vecSensors) {
               pc_sensor->Update();
               if(m_bSignalRaised) {
                  THROW_ARGOSEXCEPTION(m_strSignal << " signal raised during sensor update");
               }
            }
            /* step the provided controller */
            m_pcController->ControlStep();
            /* check for errors */
            if(!m_pcController->IsOK()) {
               THROW_ARGOSEXCEPTION("Controller: " << m_pcController->GetErrorMessage());
            }
            /* actuator update */
            /* TODO: assuming this doesn't break anything, the act phase should be called before the step phase */
            for(CPhysicalActuator* pc_actuator : m_vecActuators) {
               pc_actuator->Update();
               if(m_bSignalRaised) {
                  THROW_ARGOSEXCEPTION(m_strSignal << " signal raised during actuator update");
               }
            }
            /* flush the logs */
            LOG.Flush();
            LOGERR.Flush();
            /* sleep if required */
            cRate.Sleep();
         }
      }
      catch(CARGoSException& ex) {
         LOG << "[NOTE] Control loop aborted" << std::endl
                << ex.what() << std::endl;
         LOG.Flush();
         return;
      }
   }

   /****************************************/
   /****************************************/

   void CDrone::Destroy() {
      /* disable the controller */
      m_pcController->Destroy();
      /* delete actuators */
      for(CPhysicalActuator* pc_actuator : m_vecActuators)
         delete pc_actuator;
      /* delete sensors */
      for(CPhysicalSensor* pc_sensor : m_vecSensors)
         delete pc_sensor;
      /* delete the IIO library's context */
      iio_context_destroy(m_psContext);
      /* remove triggers */
      std::ofstream cRemoveTrigger;
      cRemoveTrigger.open(REMOVE_TRIGGER_PATH);
      cRemoveTrigger << std::to_string(SENSOR_TRIGGER_IDX) << std::flush;
      cRemoveTrigger.close();
      LOG << "[INFO] Controller terminated" << std::endl;
   }

   /****************************************/
   /****************************************/

   CDrone::CPixhawk::CPixhawk() :
      m_nFileDescriptor(-1) {}

   void CDrone::CPixhawk::Open(const std::string& str_device, SInt32 n_baud) {
      try {
         m_nFileDescriptor = 
            ::open(str_device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
         if (m_nFileDescriptor < 0)
            THROW_ARGOSEXCEPTION("Could not open " << str_device);
         ::fcntl(m_nFileDescriptor, F_SETFL, 0);
         if(!::isatty(m_nFileDescriptor)) {
            THROW_ARGOSEXCEPTION(str_device << " is not a serial port");
         }
         struct termios sPortConfiguration;
         if(::tcgetattr(m_nFileDescriptor, &sPortConfiguration) < 0) {
            THROW_ARGOSEXCEPTION("Could not read port configuration");
         }
         sPortConfiguration.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
         sPortConfiguration.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
#ifdef OLCUC
         sPortConfiguration.c_oflag &= ~OLCUC;
#endif
#ifdef ONOEOT
         sPortConfiguration.c_oflag &= ~ONOEOT;
#endif
         sPortConfiguration.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
         sPortConfiguration.c_cflag &= ~(CSIZE | PARENB);
         sPortConfiguration.c_cflag |= CS8;
         /* always return immediately regardless of whether a character was available or not */
         sPortConfiguration.c_cc[VMIN]  = 0;
         sPortConfiguration.c_cc[VTIME] = 0;
         /* apply baudrate */
         ::speed_t nBaudRate = B0;
         switch(n_baud) {
            case 50: nBaudRate = B50; break;
            case 75: nBaudRate = B75; break;
            case 110: nBaudRate = B110; break;
            case 134: nBaudRate = B134; break;
            case 150: nBaudRate = B150; break;
            case 200: nBaudRate = B200; break;
            case 300: nBaudRate = B300; break;
            case 600: nBaudRate = B600; break;
            case 1200: nBaudRate = B1200; break;
            case 1800: nBaudRate = B1800; break;
            case 2400: nBaudRate = B2400; break;
            case 4800: nBaudRate = B4800; break;
            case 9600: nBaudRate = B9600; break;
            case 19200: nBaudRate = B19200; break;
            case 38400: nBaudRate = B38400; break;
            case 57600: nBaudRate = B57600; break;
            case 115200: nBaudRate = B115200; break;
            case 230400: nBaudRate = B230400; break;
            case 460800: nBaudRate = B460800; break;
            case 500000: nBaudRate = B500000; break;
            case 576000: nBaudRate = B576000; break;
            case 921600: nBaudRate = B921600; break;
            case 1000000: nBaudRate = B1000000; break;
            case 1152000: nBaudRate = B1152000; break;
            case 1500000: nBaudRate = B1500000; break;
            case 2000000: nBaudRate = B2000000; break;
            case 2500000: nBaudRate = B2500000; break;
            case 3000000: nBaudRate = B3000000; break;
            case 3500000: nBaudRate = B3500000; break;
            case 4000000: nBaudRate = B4000000; break;
            default: THROW_ARGOSEXCEPTION(n_baud << " is not a recognized baudrate"); break;
         }
         if (::cfsetispeed(&sPortConfiguration, nBaudRate) < 0 || ::cfsetospeed(&sPortConfiguration, nBaudRate) < 0) {
            THROW_ARGOSEXCEPTION("Could not set baudrate to " << n_baud);
         }
         if(::tcsetattr(m_nFileDescriptor, TCSAFLUSH, &sPortConfiguration) < 0) {
            THROW_ARGOSEXCEPTION("Could not write port configuration");
         }
      }
      catch(CARGoSException &ex) {
         THROW_ARGOSEXCEPTION_NESTED("Could not open the MAVLink channel", ex);
      }
   }

   void CDrone::CPixhawk::Close() {
      if(m_nFileDescriptor != -1) {
         ::close(m_nFileDescriptor);
         m_nFileDescriptor = -1;
      }
   }

   int CDrone::CPixhawk::GetFileDescriptor() {
      return m_nFileDescriptor;
   }

   std::optional<CVector3>& CDrone::CPixhawk::GetInitialPosition() {
      return m_optInitialPosition;
   }

   std::optional<CVector3>& CDrone::CPixhawk::GetInitialOrientation() {
      return m_optInitialOrientation;
   }

   std::optional<uint8_t>& CDrone::CPixhawk::GetTargetSystem() {
      return m_optTargetSystem;

   }

   std::optional<uint8_t>& CDrone::CPixhawk::GetTargetComponent() {
      return m_optTargetComponent;
   }

   bool CDrone::CPixhawk::Ready() {
      return (m_nFileDescriptor != -1) &&
             m_optInitialPosition &&
             m_optInitialOrientation &&
             m_optTargetSystem &&
             m_optTargetComponent;
   }

   /****************************************/
   /****************************************/

}
