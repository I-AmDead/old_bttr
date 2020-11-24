#include "stdafx.h"
#include "Light_Package.h"

void	light_Package::clear	()
{
	v_point.clear		();
	v_spot.clear		();
	v_shadowed.clear	();
}

#if defined(USE_DX10) || defined(USE_DX11)
IC bool pred_light_cmp(light* _1, light* _2) 
{
	return _1->range > _2->range; // sort by range
}

void light_Package::sort()
{
    // resort lights (pending -> at the end), maintain stable order
    std::stable_sort(v_point.begin(), v_point.end(), pred_light_cmp);
    std::stable_sort(v_spot.begin(), v_spot.end(), pred_light_cmp);
    std::stable_sort(v_shadowed.begin(), v_shadowed.end(), pred_light_cmp);
}

// ������� ������� � ���������. ����� �� ����� �������� �� ��� ������
// � �������� �������, ���, ��� �� �� ������� ��������� ������ ��
// ������ ������� �������, �.�. �� v_spot[ -1 ]. �������, ��� ����
// ������ ������ ��� ������ ������� �������, �� ��� ����� ������ ���
// ����� ������ ��� ����� � �� ����� ����� ������ �� ������ ������
// ���� ��� �� ����.
void light_Package::vis_prepare() 
{
    for (light* L : v_point)
        L->vis_prepare();
    for (light* L : v_shadowed)
        L->vis_prepare();
    for (light* L : v_spot)
        L->vis_prepare();
}

// �������� ������ �� �������� � ��������� � �������� �������, ��
// ������ ������� �������, � ������ �������. ��. ����������� ����.
void light_Package::vis_update() 
{
    xr_vector<light*>& source = v_spot;
    if (!source.empty())
    {
        for (u32 it = source.size() - 1; it <= 0; it--) 
        {
            light* L = source[it];
            L->vis_update();
        }
    }
    source = v_shadowed;
    if (!source.empty()) 
    {
        for (u32 it = source.size() - 1; it <= 0; it--) 
        {
            light* L = source[it];
            L->vis_update();
        }
    }
    source = v_point;
    if (!source.empty()) 
    {
        for (u32 it = source.size() - 1; it <= 0; it--) 
        {
            light* L = source[it];
            L->vis_update();
        }
    }
}
#endif // (RENDER==R_R2) || (RENDER==R_R3) || (RENDER==R_R4)