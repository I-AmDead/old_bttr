#pragma once

// OldSerpskiStalker
class ExteranData
{
public:
	// ���� ���� � �������, ����������� � �������� �����
	bool					is_zoomed;

	void					ZoomActive(bool zoom_only) 
	{ 
		is_zoomed = zoom_only; 
	};

	bool					IsZoomActive() 
	{ 
		return is_zoomed;
	};

	// �������� �������, ������ �� ��������������
	bool					is_reload;

	void					ReloadActive(bool reload_only)
	{
		is_reload = reload_only;
	};

	bool					IsReloadActive()
	{
		return is_reload;
	};
};