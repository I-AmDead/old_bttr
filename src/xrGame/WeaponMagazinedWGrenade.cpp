#include "stdafx.h"
#include "weaponmagazinedwgrenade.h"
#include "entity.h"
#include "ParticlesObject.h"
#include "GrenadeLauncher.h"
#include "xrserver_objects_alife_items.h"
#include "ExplosiveRocket.h"
#include "Actor.h"
#include "xr_level_controller.h"
#include "level.h"
#include "object_broker.h"
#include "game_base_space.h"
#include "../xrphysics/MathUtils.h"
#include "player_hud.h"

#include "../build_engine_config.h"

#ifdef DEBUG
#	include "phdebug.h"
#endif

extern int g_sprint_reload_wpn;

CWeaponMagazinedWGrenade::CWeaponMagazinedWGrenade(ESoundTypes eSoundType) : CWeaponMagazined(eSoundType)
{
}

CWeaponMagazinedWGrenade::~CWeaponMagazinedWGrenade()
{}

void CWeaponMagazinedWGrenade::Load(LPCSTR section)
{
    inherited::Load(section);
    CRocketLauncher::Load(section);

    //// Sounds
    m_sounds.LoadSound(section, "snd_shoot_grenade", "sndShotG", true, m_eSoundShot);
    m_sounds.LoadSound(section, "snd_reload_grenade", "sndReloadG", true, m_eSoundReload);
    m_sounds.LoadSound(section, "snd_switch", "sndSwitch", true, m_eSoundReload);

    m_sFlameParticles2 = pSettings->r_string(section, "grenade_flame_particles");

    m_ammoTypes2.clear();
	if (m_eGrenadeLauncherStatus != ALife::eAddonAttachable)
	{
		LPCSTR S = pSettings->r_string(section, "grenade_class");
		if (S && S[0])
		{
			string128 _ammoItem;
			int count = _GetItemCount(S);
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _ammoItem);
				m_ammoTypes2.push_back(_ammoItem);
			}
		}
	}

    if (m_eGrenadeLauncherStatus == ALife::eAddonPermanent)
    {
        CRocketLauncher::m_fLaunchSpeed = pSettings->r_float(section, "grenade_vel");
    }

    //Kondr48
    magMaxSize1 = iMagazineSize;
    magMaxSize2 = iMagazineSize2;
}

void CWeaponMagazinedWGrenade::net_Destroy()
{
    inherited::net_Destroy();
}

void CWeaponMagazinedWGrenade::UpdateSecondVP(bool bInGrenade)
{
	inherited::UpdateSecondVP(m_bGrenadeMode);
}

BOOL CWeaponMagazinedWGrenade::net_Spawn(CSE_Abstract* DC)
{
    CSE_ALifeItemWeapon* const weapon = smart_cast<CSE_ALifeItemWeapon*>(DC);
    R_ASSERT(weapon);
	
//    inherited::net_Spawn_install_upgrades(weapon->m_upgrades);

    BOOL l_res = inherited::net_Spawn(DC);

	if (m_eGrenadeLauncherStatus == ALife::eAddonAttachable)
	{
		m_ammoTypes2.clear();
		LPCSTR S = pSettings->r_string(GetGrenadeLauncherName(), "grenade_class");
		if (S && S[0])
		{
			string128 _ammoItem;
			int count = _GetItemCount(S);
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _ammoItem);
				m_ammoTypes2.push_back(_ammoItem);
			}
		}
	}

	if (m_ammoType.type2 >= m_ammoTypes2.size())
		m_ammoType.type2 = 0;

	m_DefaultCartridge2.Load(m_ammoTypes2[m_ammoType.type2].c_str(), m_ammoType.type2);
	if (m_ammoElapsed.type2)
	{
		for (int i = 0; i < m_ammoElapsed.type2; ++i)
			m_magazine2.push_back(m_DefaultCartridge2);

		if (!getRocketCount())
		{
			shared_str fake_grenade_name = pSettings->r_string(m_magazine2.back().m_ammoSect, "fake_grenade_name");
			CRocketLauncher::SpawnRocket(*fake_grenade_name, this);
		}
	}

	UpdateGrenadeVisibility(m_bGrenadeMode && !!m_ammoElapsed.type2?true:false);
	SetPending(FALSE);

	if (m_bGrenadeMode && IsGrenadeLauncherAttached())
	{
		swap(iMagazineSize, iMagazineSize2);
		u8 old = m_ammoType.type1;
		m_ammoType.type1 = m_ammoType.type2;
		m_ammoType.type2 = old;
		m_ammoTypes.swap(m_ammoTypes2);
		swap(m_DefaultCartridge, m_DefaultCartridge2);
		m_magazine.swap(m_magazine2);
		m_ammoElapsed.type1 = (u16)m_magazine.size();
		m_ammoElapsed.type2 = (u16)m_magazine2.size();
	}
	else //In the case of config change or upgrade that removes GL
	{
		m_bGrenadeMode = false;
	}

    return l_res;
}

void CWeaponMagazinedWGrenade::switch2_Reload()
{
    VERIFY(GetState() == eReload);
    if (m_bGrenadeMode)
    {
        PlaySound("sndReloadG", get_LastFP2());

        PlayHUDMotion("anm_reload_g", FALSE, this, GetState());
        SetPending(TRUE);
    }
    else
        inherited::switch2_Reload();
}

void CWeaponMagazinedWGrenade::OnShot()
{
    if (m_bGrenadeMode)
    {
        PlayAnimShoot();
        PlaySound("sndShotG", get_LastFP2());
        AddShotEffector();
        StartFlameParticles2();
    }
    else
        inherited::OnShot();
}

bool CWeaponMagazinedWGrenade::SwitchMode()
{
    bool bUsefulStateToSwitch = ((eIdle == GetState()) || (eHidden == GetState()) || (eMisfire == GetState()) || (eMagEmpty == GetState())) && (!IsPending());

    if (!bUsefulStateToSwitch)
        return false;

    if (!IsGrenadeLauncherAttached())
        return false;

    OnZoomOut();

    SetPending(TRUE);

    PerformSwitchGL();

    PlaySound("sndSwitch", get_LastFP());

    PlayAnimModeSwitch();

    m_BriefInfo_CalcFrame = 0;

    return					true;
}

void  CWeaponMagazinedWGrenade::PerformSwitchGL()
{
    m_bGrenadeMode = !m_bGrenadeMode;

    swap(iMagazineSize, iMagazineSize2);

    m_ammoTypes.swap(m_ammoTypes2);

    u8 old = m_ammoType.type1;
	m_ammoType.type1 = m_ammoType.type2;
	m_ammoType.type2 = old;
	
    swap(m_DefaultCartridge, m_DefaultCartridge2);

    m_magazine.swap(m_magazine2);
    m_ammoElapsed.type1 = (u16)m_magazine.size();
	m_ammoElapsed.type2 = (u16)m_magazine2.size();

    m_BriefInfo_CalcFrame = 0;
}

bool CWeaponMagazinedWGrenade::Action(u16 cmd, u32 flags)
{
    if (m_bGrenadeMode && cmd == kWPN_FIRE)
    {
        if (IsPending())
            return				false;

        if (flags&CMD_START)
        {
            if (m_ammoElapsed.type1)
                LaunchGrenade();
            else
                Reload();

            if (GetState() == eIdle)
                OnEmptyClick();
        }
        return					true;
    }
    if (inherited::Action(cmd, flags))
        return true;

    switch (cmd)
    {
    case kWPN_FUNC:
    {
        if (flags&CMD_START && !IsPending())
            SwitchState(eSwitch);
        return true;
    }
    }
    return false;
}

#include "inventory.h"
#include "inventoryOwner.h"
void CWeaponMagazinedWGrenade::state_Fire(float dt)
{
    VERIFY(fOneShotTime > 0.f);

    //режим стрельбы подствольника
    if (m_bGrenadeMode)
    {

    }
    //режим стрельбы очередями
    else
        inherited::state_Fire(dt);
}

void CWeaponMagazinedWGrenade::OnEvent(NET_Packet& P, u16 type)
{
    inherited::OnEvent(P, type);
    u16 id;
    switch (type)
    {
    case GE_OWNERSHIP_TAKE:
    {
        P.r_u16(id);
        CRocketLauncher::AttachRocket(id, this);
    }
    break;
    case GE_OWNERSHIP_REJECT:
    case GE_LAUNCH_ROCKET:
    {
        bool bLaunch = (type == GE_LAUNCH_ROCKET);
        P.r_u16(id);
        CRocketLauncher::DetachRocket(id, bLaunch);
        if (bLaunch)
        {
            PlayAnimShoot();
            PlaySound("sndShotG", get_LastFP2());
            AddShotEffector();
            StartFlameParticles2();
        }
        break;
    }
    }
}

void  CWeaponMagazinedWGrenade::LaunchGrenade()
{
    if (!getRocketCount())	return;
    R_ASSERT(m_bGrenadeMode);
    {
        Fvector						p1, d;
        p1.set(get_LastFP2());
        d.set(get_LastFD());
        CEntity*					E = smart_cast<CEntity*>(H_Parent());

        if (E)
        {
            CInventoryOwner* io = smart_cast<CInventoryOwner*>(H_Parent());
            if (NULL == io->inventory().ActiveItem())
            {
                Log("current_state", GetState());
                Log("next_state", GetNextState());
                Log("item_sect", cNameSect().c_str());
                Log("H_Parent", H_Parent()->cNameSect().c_str());
            }
            E->g_fireParams(this, p1, d);
        }
        if (IsGameTypeSingle())
            p1.set(get_LastFP2());

        Fmatrix							launch_matrix;
        launch_matrix.identity();
        launch_matrix.k.set(d);
        Fvector::generate_orthonormal_basis(launch_matrix.k,
            launch_matrix.j,
            launch_matrix.i);

        launch_matrix.c.set(p1);

        if (IsGameTypeSingle() && IsZoomed() && smart_cast<CActor*>(H_Parent()))
        {
            H_Parent()->setEnabled(FALSE);
            setEnabled(FALSE);

            collide::rq_result			RQ;
            BOOL HasPick = Level().ObjectSpace.RayPick(p1, d, 300.0f, collide::rqtStatic, RQ, this);

            setEnabled(TRUE);
            H_Parent()->setEnabled(TRUE);

            if (HasPick)
            {
                Fvector					Transference;
                Transference.mul(d, RQ.range);
                Fvector					res[2];
#ifdef		DEBUG
                //.				DBG_OpenCashedDraw();
                //.				DBG_DrawLine(p1,Fvector().add(p1,d),D3DCOLOR_XRGB(255,0,0));
#endif
                u8 canfire0 = TransferenceAndThrowVelToThrowDir(Transference,
                    CRocketLauncher::m_fLaunchSpeed,
                    EffectiveGravity(),
                    res);
#ifdef DEBUG
                //.				if(canfire0>0)DBG_DrawLine(p1,Fvector().add(p1,res[0]),D3DCOLOR_XRGB(0,255,0));
                //.				if(canfire0>1)DBG_DrawLine(p1,Fvector().add(p1,res[1]),D3DCOLOR_XRGB(0,0,255));
                //.				DBG_ClosedCashedDraw(30000);
#endif

                if (canfire0 != 0)
                {
                    d = res[0];
                };
            }
        };

        d.normalize();
        d.mul(CRocketLauncher::m_fLaunchSpeed);
        VERIFY2(_valid(launch_matrix), "CWeaponMagazinedWGrenade::SwitchState. Invalid launch_matrix!");
        CRocketLauncher::LaunchRocket(launch_matrix, d, zero_vel);

        CExplosiveRocket* pGrenade = smart_cast<CExplosiveRocket*>(getCurrentRocket());
        VERIFY(pGrenade);
        pGrenade->SetInitiator(H_Parent()->ID());
		pGrenade->SetRealGrenadeName(m_ammoTypes[m_ammoType.type1]);
		
        if (Local() && OnServer())
        {
            VERIFY(m_magazine.size());
            m_magazine.pop_back();
            --m_ammoElapsed.type1;
            VERIFY((u32) m_ammoElapsed.type1 == m_magazine.size());

            NET_Packet					P;
            u_EventGen(P, GE_LAUNCH_ROCKET, ID());
            P.w_u16(getCurrentRocket()->ID());
            u_EventSend(P);
        };
    }
}

void CWeaponMagazinedWGrenade::FireEnd()
{
    if (m_bGrenadeMode)
    {
        CWeapon::FireEnd();
    }
    else
        inherited::FireEnd();
}

void CWeaponMagazinedWGrenade::OnMagazineEmpty()
{
    if (GetState() == eIdle)
    {
        OnEmptyClick();
    }
}

void CWeaponMagazinedWGrenade::ReloadMagazine()
{
    inherited::ReloadMagazine();

    //перезарядка подствольного гранатомета
    if (m_ammoElapsed.type1 && !getRocketCount() && m_bGrenadeMode)
    {
        shared_str fake_grenade_name = pSettings->r_string(m_ammoTypes[m_ammoType.type1].c_str(), "fake_grenade_name");

        if (g_sprint_reload_wpn && smart_cast<CActor*>(H_Parent()) != NULL)
            Actor()->SetCantRunState(true); 

        CRocketLauncher::SpawnRocket(*fake_grenade_name, this);
    }
}

void CWeaponMagazinedWGrenade::OnStateSwitch(u32 S, u32 oldState)
{
    switch (S)
    {
    case eSwitch:
    {
        if (!SwitchMode())
        {
            SwitchState(eIdle);
            return;
        }
    }break;
    }

    inherited::OnStateSwitch(S, oldState);
    UpdateGrenadeVisibility(!!m_ammoElapsed.type1 || S == eReload);
}

void CWeaponMagazinedWGrenade::OnAnimationEnd(u32 state)
{
    switch (state)
    {
    case eSwitch:
    {
        SwitchState(eIdle);
    }break;
    case eFire:
    {
        if (m_bGrenadeMode)
            Reload();

        if (g_sprint_reload_wpn && smart_cast<CActor*>(H_Parent()) != NULL)
            Actor()->SetCantRunState(false);
    }break;
    }
    inherited::OnAnimationEnd(state);
}

void CWeaponMagazinedWGrenade::OnH_B_Independent(bool just_before_destroy)
{
    inherited::OnH_B_Independent(just_before_destroy);

    SetPending(FALSE);
    if (m_bGrenadeMode)
    {
        SetState(eIdle);
        SetPending(FALSE);
    }
}

//Kondr48
void CWeaponMagazinedWGrenade::Chamber()
{
    if (!m_bGrenadeMode)
    {
        if (m_bChamberStatus == true && m_ammoElapsed.type1 >= 1 && m_chamber == false)
        {
            m_chamber = true;
            iMagazineSize++;
            magMaxSize1++;
        }
        else if (m_bChamberStatus == true && m_ammoElapsed.type1 == 0 && m_chamber == true)
        {
            m_chamber = false;
            iMagazineSize--;
            magMaxSize1--;
        }
    }
}

bool CWeaponMagazinedWGrenade::CanAttach(PIItem pIItem)
{
    return inherited::CanAttach(pIItem);
}

bool CWeaponMagazinedWGrenade::CanDetach(LPCSTR item_section_name)
{
    return inherited::CanDetach(item_section_name);
}

bool CWeaponMagazinedWGrenade::Attach(PIItem pIItem, bool b_send_event)
{
    CGrenadeLauncher* pGrenadeLauncher = smart_cast<CGrenadeLauncher*>(pIItem);

    if (pGrenadeLauncher &&
		m_eGrenadeLauncherStatus == ALife::eAddonAttachable &&
		(m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) == 0 /*&&
																					(m_sGrenadeLauncherName == pIItem->object().cNameSect())*/)
	{
		SCOPES_VECTOR_IT it = m_launchers.begin();
		for (; it != m_launchers.end(); it++)
		{
			if (pSettings->r_string((*it), "grenade_launcher_name") == pIItem->object().cNameSect())
			{
				m_cur_addon.launcher = u16(it - m_launchers.begin());

				m_flagsAddOnState |= CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher;
				CRocketLauncher::m_fLaunchSpeed = pGrenadeLauncher->GetGrenadeVel();

				m_ammoTypes2.clear();
				LPCSTR S = pSettings->r_string(pIItem->object().cNameSect(), "grenade_class");
				if (S && S[0])
				{
					string128 _ammoItem;
					int count = _GetItemCount(S);
					for (int it = 0; it < count; ++it)
					{
						_GetItem(S, it, _ammoItem);
						m_ammoTypes2.push_back(_ammoItem);
					}
				}

				//oie?oi?eou iianoaieuiee ec eiaaioa?y
				if (b_send_event)
				{
					if (OnServer())
						pIItem->object().DestroyObject();
				}
				InitAddons();
				UpdateAddonsVisibility();

				if (GetState() == eIdle)
					PlayAnimIdle();

				SyncronizeWeaponToServer();

				return					true;
			}
		}
	}
    return inherited::Attach(pIItem, b_send_event);
}

bool CWeaponMagazinedWGrenade::Detach(LPCSTR item_section_name, bool b_spawn_item)
{
	if (m_eGrenadeLauncherStatus == ALife::eAddonAttachable &&
		DetachGrenadeLauncher(item_section_name, b_spawn_item))
	{
		if ((m_flagsAddOnState & CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) == 0)
		{
			Msg("ERROR: grenade launcher addon already detached.");
			return true;
		}
		m_flagsAddOnState &= ~CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher;

		// Now we need to unload GL's magazine
		if (!m_bGrenadeMode)
		{
			PerformSwitchGL();
		}
		UnloadMagazine();
		PerformSwitchGL();

		UpdateAddonsVisibility();

		if (GetState() == eIdle)
			PlayAnimIdle();

		SyncronizeWeaponToServer();

		return CInventoryItemObject::Detach(item_section_name, b_spawn_item);
	}
	else
		return inherited::Detach(item_section_name, b_spawn_item);
}

void CWeaponMagazinedWGrenade::InitAddons()
{
    inherited::InitAddons();

    if (IsGrenadeLauncherAttached() && GrenadeLauncherAttachable())
	{
		CRocketLauncher::m_fLaunchSpeed = pSettings->r_float(GetGrenadeLauncherName(), "grenade_vel");

		m_ammoTypes2.clear();
		LPCSTR S = pSettings->r_string(GetGrenadeLauncherName(), "grenade_class");
		if (S && S[0])
		{
			string128 _ammoItem;
			int count = _GetItemCount(S);
			for (int it = 0; it < count; ++it)
			{
				_GetItem(S, it, _ammoItem);
				m_ammoTypes2.push_back(_ammoItem);
			}
		}
	}
}

bool	CWeaponMagazinedWGrenade::UseScopeTexture()
{
    if (IsGrenadeLauncherAttached() && m_bGrenadeMode) return false;

    return true;
};

float	CWeaponMagazinedWGrenade::CurrentZoomFactor()
{
    if (IsGrenadeLauncherAttached() && m_bGrenadeMode) return m_zoom_params.m_fIronSightZoomFactor;
    return inherited::CurrentZoomFactor();
}

//виртуальные функции для проигрывания анимации HUD
void CWeaponMagazinedWGrenade::PlayAnimShow()
{
    VERIFY(GetState() == eShowing);
    if (IsGrenadeLauncherAttached())
    {
        if (!m_bGrenadeMode)
            PlayHUDMotion("anm_show_w_gl", FALSE, this, GetState());
        else
            PlayHUDMotion("anm_show_g", FALSE, this, GetState());
    }
    else
        PlayHUDMotion("anm_show", FALSE, this, GetState());
}

void CWeaponMagazinedWGrenade::PlayAnimHide()
{
    VERIFY(GetState() == eHiding);

    if (IsGrenadeLauncherAttached())
        if (!m_bGrenadeMode)
            PlayHUDMotion("anm_hide_w_gl", TRUE, this, GetState());
        else
            PlayHUDMotion("anm_hide_g", TRUE, this, GetState());

    else
        PlayHUDMotion("anm_hide", TRUE, this, GetState());
}

void CWeaponMagazinedWGrenade::PlayAnimIdle()
{
    if (IsGrenadeLauncherAttached())
    {
        CActor* pActor = smart_cast<CActor*>(H_Parent());
        if (pActor)
        {
            CEntity::SEntityState st;
            pActor->g_State(st);
            if (IsZoomed())
            {
                if (pActor->AnyMove())
                {
                    if (HudAnimationExist("anm_idle_g_aim_moving") && HudAnimationExist("anm_idle_w_gl_aim_moving"))
                    {
                        if (m_bGrenadeMode)
                            PlayHUDMotion("anm_idle_g_aim_moving", TRUE, NULL, GetState());
                        else
                            PlayHUDMotion("anm_idle_w_gl_aim_moving", TRUE, NULL, GetState());
                    }
                    else
                    {
                        if (m_bGrenadeMode)
                            PlayHUDMotion("anm_idle_g_aim", TRUE, NULL, GetState());
                        else
                            PlayHUDMotion("anm_idle_w_gl_aim", TRUE, NULL, GetState());
                    }
                }
                else
                {
                    if (m_bGrenadeMode)
                        PlayHUDMotion("anm_idle_g_aim", TRUE, NULL, GetState());
                    else
                        PlayHUDMotion("anm_idle_w_gl_aim", TRUE, NULL, GetState());
                }
            }
            else
            {
                int act_state = 0;
                if (st.bSprint)
                {
                    act_state = 1;		// Sprint
                }
                else if (!st.bCrouch && pActor->AnyMove() && !pActor->Accel())
                {
                    act_state = 2;		// Moving
                }
                else if (!st.bCrouch && pActor->Accel() && pActor->AnyMove())
                {
                    act_state = 3;		// Moving Slow
                }
                else if (st.bCrouch && pActor->AnyMove() && !pActor->Accel() || pActor->Crouch() && pActor->Accel() && pActor->AnyMove())
                {
                    act_state = 4;		// Crouch
                }

                if (m_bGrenadeMode)
                {
                    if (act_state == 0)
                        PlayHUDMotion("anm_idle_g", /*FALSE*/TRUE, NULL, GetState()); //AVO: fix fast anim switch
                    else
                        if (act_state == 1)
                            PlayHUDMotion("anm_idle_sprint_g", TRUE, NULL, GetState());
                        else
                            if (act_state == 2)
                                PlayHUDMotion("anm_idle_moving_g", TRUE, NULL, GetState());
                            else
                                if (act_state == 3)
                                {
                                    if (HudAnimationExist("anm_idle_moving_slow_g"))
                                        PlayHUDMotion("anm_idle_moving_slow_g", TRUE, NULL, GetState());
                                    else
                                        PlayHUDMotion("anm_idle_moving_g", TRUE, NULL, GetState());
                                }
                                else
                                    if (act_state == 4)
                                    {
                                        if (HudAnimationExist("anm_idle_moving_crouch_g"))
                                            PlayHUDMotion("anm_idle_moving_crouch_g", TRUE, NULL, GetState());
                                        else
                                            PlayHUDMotion("anm_idle_moving_g", TRUE, NULL, GetState());
                                    }
                }
                else
                {
                    if (act_state == 0)
                        PlayHUDMotion("anm_idle_w_gl", /*FALSE*/TRUE, NULL, GetState()); //AVO: fix fast anim switch
                    else
                        if (act_state == 1)
                            PlayHUDMotion("anm_idle_sprint_w_gl", TRUE, NULL, GetState());
                        else
                            if (act_state == 2)
                                PlayHUDMotion("anm_idle_moving_w_gl", TRUE, NULL, GetState());
                            else
                                if (act_state == 3)
                                {
                                    if (HudAnimationExist("anm_idle_moving_slow_w_gl"))
                                        PlayHUDMotion("anm_idle_moving_slow_w_gl", TRUE, NULL, GetState());
                                    else
                                        PlayHUDMotion("anm_idle_moving_w_gl", TRUE, NULL, GetState());
                                }
                                else
                                    if (act_state == 4)
                                    {
                                        if (HudAnimationExist("anm_idle_moving_crouch_w_gl"))
                                            PlayHUDMotion("anm_idle_moving_crouch_w_gl", TRUE, NULL, GetState());
                                        else
                                            PlayHUDMotion("anm_idle_moving_w_gl", TRUE, NULL, GetState());
                                    }
                }
            }
        }
    }
    else
        inherited::PlayAnimIdle();
}

void CWeaponMagazinedWGrenade::PlayAnimShoot()
{
    if (m_bGrenadeMode)
    {
        PlayHUDMotion("anm_shots_g", FALSE, this, eFire);
    }
    else
    {
        VERIFY(GetState() == eFire);
        if (IsGrenadeLauncherAttached())
        {
            if (IsZoomed())
            {
                if (HudAnimationExist("anm_shots_w_gl_when_aim"))
                    PlayHUDMotion("anm_shots_w_gl_when_aim", FALSE, this, GetState());
                else
                    PlayHUDMotion("anm_shots_w_gl", FALSE, this, GetState());
            }
            else
            {
                PlayHUDMotion("anm_shots_w_gl", FALSE, this, GetState());
            }
        }
        else
            inherited::PlayAnimShoot();
    }
}

void  CWeaponMagazinedWGrenade::PlayAnimModeSwitch()
{
    if (m_bGrenadeMode)
        PlayHUDMotion("anm_switch_g", /*FALSE*/ TRUE, this, eSwitch); //AVO: fix fast anim switch
    else
        PlayHUDMotion("anm_switch", /*FALSE*/ TRUE, this, eSwitch); //AVO: fix fast anim switch
}

void CWeaponMagazinedWGrenade::PlayAnimBore()
{
    if (IsGrenadeLauncherAttached())
    {
        if (m_bGrenadeMode)
            PlayHUDMotion("anm_bore_g", TRUE, this, GetState());
        else
            PlayHUDMotion("anm_bore_w_gl", TRUE, this, GetState());
    }
    else
        inherited::PlayAnimBore();
}

void CWeaponMagazinedWGrenade::UpdateSounds()
{
    inherited::UpdateSounds();
    Fvector P = get_LastFP();
    m_sounds.SetPosition("sndShotG", P);
    m_sounds.SetPosition("sndReloadG", P);
    m_sounds.SetPosition("sndSwitch", P);
}

void CWeaponMagazinedWGrenade::UpdateGrenadeVisibility(bool visibility)
{
    if (!GetHUDmode())							return;
    HudItemData()->set_bone_visible("grenade", visibility, TRUE);
}

void CWeaponMagazinedWGrenade::save(NET_Packet &output_packet)
{
    inherited::save(output_packet);
    save_data(m_bGrenadeMode, output_packet);
}

void CWeaponMagazinedWGrenade::load(IReader &input_packet)
{
    inherited::load(input_packet);
    load_data(m_bGrenadeMode, input_packet);
}

void CWeaponMagazinedWGrenade::net_Export(NET_Packet& P)
{
    P.w_u8(m_bGrenadeMode ? 1 : 0);
    inherited::net_Export(P);
}

void CWeaponMagazinedWGrenade::net_Import(NET_Packet& P)
{
    m_bGrenadeMode = !!P.r_u8();
    inherited::net_Import(P);
}

float CWeaponMagazinedWGrenade::Weight() const
{
    float res = inherited::Weight();
    res += GetMagazineWeight(m_magazine2);
	
    return res;
}

bool CWeaponMagazinedWGrenade::IsNecessaryItem(const shared_str& item_sect)
{
    return (std::find(m_ammoTypes.begin(), m_ammoTypes.end(), item_sect) != m_ammoTypes.end() ||
        std::find(m_ammoTypes2.begin(), m_ammoTypes2.end(), item_sect) != m_ammoTypes2.end()
        );
}

u8 CWeaponMagazinedWGrenade::GetCurrentHudOffsetIdx()
{
    bool b_aiming = ((IsZoomed() && m_zoom_params.m_fZoomRotationFactor <= 1.f) ||
        (!IsZoomed() && m_zoom_params.m_fZoomRotationFactor > 0.f));

    if (!b_aiming)
        return		0;
    else
        if (m_bGrenadeMode)
            return		2;
        else
            return		1;
}

bool CWeaponMagazinedWGrenade::install_upgrade_ammo_class(LPCSTR section, bool test)
{
    LPCSTR str;

    bool result = process_if_exists(section, "ammo_mag_size", &CInifile::r_s32, m_bGrenadeMode?iMagazineSize2:iMagazineSize, test);

    //	ammo_class = ammo_5.45x39_fmj, ammo_5.45x39_ap  // name of the ltx-section of used ammo
    bool result2 = process_if_exists_set(section, "ammo_class", &CInifile::r_string, str, test);
    if (result2 && !test)
    {
        xr_vector<shared_str>& ammo_types = !m_bGrenadeMode ? m_ammoTypes2 : m_ammoTypes;
        ammo_types.clear();
        for (int i = 0, count = _GetItemCount(str); i < count; ++i)
        {
            string128						ammo_item;
            _GetItem(str, i, ammo_item);
            ammo_types.push_back(ammo_item);
        }

        m_ammoType.data = 0;

		SyncronizeWeaponToServer();
    }
    result |= result2;

    return result2;
}

bool CWeaponMagazinedWGrenade::install_upgrade_impl(LPCSTR section, bool test)
{
    LPCSTR str;
    bool result = inherited::install_upgrade_impl(section, test);

    //	grenade_class = ammo_vog-25, ammo_vog-25p          // name of the ltx-section of used grenades
    bool result2 = process_if_exists_set(section, "grenade_class", &CInifile::r_string, str, test);
    if (result2 && !test)
    {
        xr_vector<shared_str>& ammo_types = !m_bGrenadeMode ? m_ammoTypes2 : m_ammoTypes;
        ammo_types.clear();
        for (int i = 0, count = _GetItemCount(str); i < count; ++i)
        {
            string128						ammo_item;
            _GetItem(str, i, ammo_item);
            ammo_types.push_back(ammo_item);
        }

        m_ammoType.data = 0;

		SyncronizeWeaponToServer();
    }
    result |= result2;

    result |= process_if_exists(section, "launch_speed", &CInifile::r_float, m_fLaunchSpeed, test);

    result2 = process_if_exists_set(section, "snd_shoot_grenade", &CInifile::r_string, str, test);
    if (result2 && !test)
    {
        m_sounds.LoadSound(section, "snd_shoot_grenade", "sndShotG", false, m_eSoundShot);
    }
    result |= result2;

    result2 = process_if_exists_set(section, "snd_reload_grenade", &CInifile::r_string, str, test);
    if (result2 && !test)
    {
        m_sounds.LoadSound(section, "snd_reload_grenade", "sndReloadG", true, m_eSoundReload);
    }
    result |= result2;

    result2 = process_if_exists_set(section, "snd_switch", &CInifile::r_string, str, test);
    if (result2 && !test)
    {
        m_sounds.LoadSound(section, "snd_switch", "sndSwitch", true, m_eSoundReload);
    }
    result |= result2;

    return result;
}

//void CWeaponMagazinedWGrenade::net_Spawn_install_upgrades(Upgrades_type saved_upgrades)
//{
    // do not delete this
    // this is intended behaviour
//}

#include "string_table.h"
bool CWeaponMagazinedWGrenade::GetBriefInfo(II_BriefInfo& info)
{
    VERIFY(m_pInventory);
    if(!inherited::GetBriefInfo(info))
		return false;

    string32 int_str;

    if (m_bGrenadeMode || !IsGrenadeLauncherAttached())
    {
        info.grenade = "";
        return false;
    }

    int total2 = GetAmmoCount2(0);
    if (unlimited_ammo())
        xr_sprintf(int_str, "--");
    else
    {
        if (total2)
            xr_sprintf(int_str, "%d", total2);
        else
            xr_sprintf(int_str, "X");
    }
    info.grenade = int_str;

    return true;
}

int CWeaponMagazinedWGrenade::GetAmmoCount2(u8 ammo2_type) const
{
    VERIFY(m_pInventory);
    R_ASSERT(ammo2_type < m_ammoTypes2.size());

    return GetAmmoCount_forType(m_ammoTypes2[ammo2_type]);
}

void CWeaponMagazinedWGrenade::PlayAnimReload()
{
    VERIFY(GetState() == eReload);

    if (IsGrenadeLauncherAttached())
    {
        if (bMisfire)
        {
            if (HudAnimationExist("anm_reload_w_gl_misfire"))
                PlayHUDMotion("anm_reload_w_gl_misfire", TRUE, this, GetState());
            else
                PlayHUDMotion("anm_reload_w_gl", TRUE, this, GetState());
        }
        else
        {
            if (m_ammoElapsed.type1 == 0)
            {
                if (HudAnimationExist("anm_reload_empty_w_gl"))
                    PlayHUDMotion("anm_reload_empty_w_gl", TRUE, this, GetState());
                else
                    PlayHUDMotion("anm_reload_w_gl", TRUE, this, GetState());
            }
            else
            {
                PlayHUDMotion("anm_reload_w_gl", TRUE, this, GetState());
            }
        }
    }
    else
        inherited::PlayAnimReload();
}

void CWeaponMagazinedWGrenade::switch2_Unmis()
{
	VERIFY(GetState() == eUnMisfire);
	
    if (!IsGrenadeLauncherAttached())
        inherited::switch2_Unmis();
    else
    {
        if (m_sounds_enabled)
        {
            if (m_sounds.FindSoundItem("sndReloadMisfire", false))
                PlaySound("sndReloadMisfire", get_LastFP());
            else if (m_sounds.FindSoundItem("sndReloadEmpty", false))
                PlaySound("sndReloadEmpty", get_LastFP());
            else
                PlaySound("sndReload", get_LastFP());
        }

        if (HudAnimationExist("anm_reload_w_gl_misfire"))
            PlayHUDMotion("anm_reload_w_gl_misfire", TRUE, this, GetState());
        else if (HudAnimationExist("anm_reload_w_gl_empty"))
            PlayHUDMotion("anm_reload_w_gl_empty", TRUE, this, GetState());
        else
            PlayHUDMotion("anm_reload_w_gl", TRUE, this, GetState());
    }
}