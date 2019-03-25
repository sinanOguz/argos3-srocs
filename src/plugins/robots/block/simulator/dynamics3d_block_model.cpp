/**
 * @file <argos3/plugins/robots/block/simulator/dynamics3d_block_model.cpp>
 *
 * @author Michael Allwright - <allsey87@gmail.com>
 */

#include "dynamics3d_block_model.h"

#include <argos3/plugins/robots/block/simulator/block_entity.h>
#include <argos3/plugins/simulator/physics_engines/dynamics3d/dynamics3d_engine.h>
#include <argos3/plugins/simulator/physics_engines/dynamics3d/dynamics3d_shape_manager.h>

#include <argos3/plugins/robots/block/simulator/block_entity.h>

namespace argos {

   /****************************************/
   /****************************************/

   CDynamics3DBlockModel::CDynamics3DBlockModel(CDynamics3DEngine& c_engine,
                                                CBlockEntity& c_block) :
      CDynamics3DSingleBodyObjectModel(c_engine, c_block),
      m_ptrBody(nullptr) {
      /* Fetch a collision shape for this model */
      const std::shared_ptr<btCollisionShape>& ptrShape = 
         CDynamics3DShapeManager::RequestBox(
            btVector3(m_fBlockSideLength * 0.5f,
                      m_fBlockSideLength * 0.5f, 
                      m_fBlockSideLength * 0.5f));
      /* Get the origin anchor */
      SAnchor& sAnchor = c_block.GetEmbodiedEntity().GetOriginAnchor();
      const CQuaternion& cOrientation = sAnchor.Orientation;
      const CVector3& cPosition = sAnchor.Position;
      /* Calculate the start transform */
      const btTransform& cStartTransform = btTransform(
         btQuaternion(cOrientation.GetX(),
                      cOrientation.GetZ(),
                     -cOrientation.GetY(),
                      cOrientation.GetW()),
         btVector3(cPosition.GetX(),
                   cPosition.GetZ(),
                  -cPosition.GetY()));
      /* Calculate the center of mass offset */
      const btTransform& cCenterOfMassOffset = btTransform(
         btQuaternion(0.0f, 0.0f, 0.0f, 1.0f),
         btVector3(0.0f, -m_fBlockSideLength * 0.5f, 0.0f));
      /* Initialize mass and inertia to zero (static object) */
      btVector3 cInertia(0.0f, 0.0f, 0.0f);
      ptrShape->calculateLocalInertia(m_fBlockMass, cInertia);
      /* Use the default friction */
      btScalar fFriction = GetEngine().GetDefaultFriction();
      /* Set up body data */
      CBody::SData sData(cStartTransform, cCenterOfMassOffset, cInertia, m_fBlockMass, fFriction);
      /* Create the body */
      m_ptrBody = std::make_shared<CBody>(*this, &sAnchor, ptrShape, sData);
      /* Transfer the body to the base class */
      m_vecBodies.push_back(m_ptrBody);
      /* Synchronize with the entity in the space */
      UpdateEntityStatus();
   }
   
   /****************************************/
   /****************************************/

   REGISTER_STANDARD_DYNAMICS3D_OPERATIONS_ON_ENTITY(CBlockEntity, CDynamics3DBlockModel);

   /****************************************/
   /****************************************/

   const Real CDynamics3DBlockModel::m_fBlockSideLength(0.055);
   const Real CDynamics3DBlockModel::m_fBlockMass(0.110);

   /****************************************/
   /****************************************/

}