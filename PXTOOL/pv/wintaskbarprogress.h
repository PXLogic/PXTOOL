/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PV_WINTASKBARPROGRESS_H
#define PV_WINTASKBARPROGRESS_H

#include <Windows.h>

struct ITaskbarList3;

namespace pv {

class WinTaskbarProgress
{
public:
	WinTaskbarProgress();
	~WinTaskbarProgress();

	WinTaskbarProgress(const WinTaskbarProgress&) = delete;
	WinTaskbarProgress& operator=(const WinTaskbarProgress&) = delete;

	static int normalizedValue(int value);

	void attach(HWND window);
	void setProgress(int value);

private:
	ITaskbarList3* _taskbar;
	HWND _window;
	bool _com_initialized;
};

} // namespace pv

#endif // PV_WINTASKBARPROGRESS_H
