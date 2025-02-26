#include "stdafx.h"
#include "poltergeist_cs.h"
#include "../../../../xrphysics/PhysicsShell.h"
#include "../../../level.h"
#include "../../../material_manager.h"
#include "../../../level_debug.h"

CPolterSpecialAbility_cs::CPolterSpecialAbility_cs(CPoltergeist_cs *polter)
{
	m_object					= polter;

	m_particles_object			= 0;
	m_particles_object_electro	= 0;
}


CPolterSpecialAbility_cs::~CPolterSpecialAbility_cs()
{
	CParticlesObject::Destroy	(m_particles_object);
	CParticlesObject::Destroy	(m_particles_object_electro);
}

void CPolterSpecialAbility_cs::load(LPCSTR section)
{
	m_particles_hidden					= pSettings->r_string(section,"Particles_Hidden");
	m_particles_damage					= pSettings->r_string(section,"Particles_Damage");
	m_particles_death					= pSettings->r_string(section,"Particles_Death");
	m_particles_idle					= pSettings->r_string(section,"Particles_Idle");

	m_sound_base.create					(pSettings->r_string(section,"Sound_Idle"), st_Effect, SOUND_TYPE_MONSTER_TALKING);

	m_last_hit_frame					= 0;
}

void CPolterSpecialAbility_cs::update_schedule()
{
	if (m_object->g_Alive()) {
		if (!m_sound_base._feedback()) m_sound_base.play_at_pos(m_object, m_object->Position());
		else m_sound_base.set_position(m_object->Position());
	}
}

void CPolterSpecialAbility_cs::on_hide()
{
	VERIFY(m_particles_object == 0);
	if (!m_object->g_Alive())
		return;
	m_particles_object			= m_object->PlayParticles	(m_particles_hidden, m_object->Position(),Fvector().set(0.0f,0.1f,0.0f), false);
	m_particles_object_electro	= m_object->PlayParticles	(m_particles_idle, m_object->Position(),Fvector().set(0.0f,0.1f,0.0f), false);
}

void CPolterSpecialAbility_cs::on_show()
{
	if (m_particles_object)			CParticlesObject::Destroy(m_particles_object);
	if (m_particles_object_electro) CParticlesObject::Destroy(m_particles_object_electro);
}

void CPolterSpecialAbility_cs::update_frame()
{
	if (m_particles_object)			m_particles_object->SetXFORM		(m_object->XFORM());
	if (m_particles_object_electro)	m_particles_object_electro->SetXFORM(m_object->XFORM());
}

void CPolterSpecialAbility_cs::on_die()
{
	Fvector particles_position	= m_object->m_current_position;
	particles_position.y		+= m_object->target_height;

	m_object->PlayParticles			(m_particles_death, particles_position, Fvector().set(0.0f,1.0f,0.0f), TRUE, FALSE);

	CParticlesObject::Destroy		(m_particles_object_electro);
	CParticlesObject::Destroy		(m_particles_object);
}

void CPolterSpecialAbility_cs::on_hit(SHit* pHDS)
{
	if (m_object->g_Alive() && (pHDS->hit_type == ALife::eHitTypeFireWound) && (Device.dwFrame != m_last_hit_frame)) {
		if(BI_NONE != pHDS->bone()) {

			//��������� ���������� ���������
			IKinematics* V = smart_cast<IKinematics*>(m_object->Visual());

			Fvector start_pos = pHDS->bone_space_position();
			Fmatrix& m_bone = V->LL_GetBoneInstance(pHDS->bone()).mTransform;
			m_bone.transform_tiny	(start_pos);
			m_object->XFORM().transform_tiny	(start_pos);

			m_object->PlayParticles(m_particles_damage, start_pos, Fvector().set(0.f,1.f,0.f));
		}
	} 

	m_last_hit_frame = Device.dwFrame;
}


//////////////////////////////////////////////////////////////////////////
// Other
//////////////////////////////////////////////////////////////////////////


#define IMPULSE					10.f
#define IMPULSE_RADIUS			5.f
#define TRACE_DISTANCE			10.f
#define TRACE_ATTEMPT_COUNT		3

void CPoltergeist_cs::PhysicalImpulse	(const Fvector &position)
{
    m_nearest.clear();
	Level().ObjectSpace.GetNearest	(m_nearest,position, IMPULSE_RADIUS, NULL); 
	//xr_vector<CObject*> &m_nearest = Level().ObjectSpace.q_nearest;
	if (m_nearest.empty())			return;
	
	u32 index = Random.randI		(m_nearest.size());
	
	CPhysicsShellHolder  *obj = smart_cast<CPhysicsShellHolder *>(m_nearest[index]);
	if (!obj || !obj->m_pPhysicsShell) return;

	Fvector dir;
	dir.sub(obj->Position(), position);
	dir.normalize();
	
	CPhysicsElement* E=obj->m_pPhysicsShell->get_ElementByStoreOrder(u16(Random.randI(obj->m_pPhysicsShell->get_ElementsNumber())));
	//E->applyImpulse(dir,IMPULSE * obj->m_pPhysicsShell->getMass());
	E->applyImpulse(dir,IMPULSE * E->getMass());
}

void CPoltergeist_cs::StrangeSounds(const Fvector &position)
{
	if (m_strange_sound._feedback()) return;
	
	for (u32 i = 0; i < TRACE_ATTEMPT_COUNT; i++) {
		Fvector dir;
		dir.random_dir();

		collide::rq_result	l_rq;
		if (Level().ObjectSpace.RayPick(position, dir, TRACE_DISTANCE, collide::rqtStatic, l_rq, NULL)) {
			if (l_rq.range < TRACE_DISTANCE) {

				// �������� ���� ����������
				CDB::TRI*	pTri	= Level().ObjectSpace.GetStaticTris() + l_rq.element;
                SGameMtlPair* mtl_pair = GMLib.GetMaterialPair(material().self_material_idx(), pTri->material);
				if (!mtl_pair) continue;

				// ������ ����
				if (!mtl_pair->CollideSounds.empty()) {
                    ref_sound& randSound = mtl_pair->CollideSounds[Random.randI(mtl_pair->CollideSounds.size())];
                    m_strange_sound.clone(randSound, st_Effect, sg_SourceType);
                    Fvector pos;
                    pos.mad(position, dir, ((l_rq.range - 0.1f > 0) ? l_rq.range - 0.1f : l_rq.range));
                    m_strange_sound.play_at_pos(this, pos);
                    return;
				}			
			}
		}
	}
}


