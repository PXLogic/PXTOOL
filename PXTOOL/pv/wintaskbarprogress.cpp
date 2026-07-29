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

#include "wintaskbarprogress.h"

#include <shobjidl.h>

namespace pv {

WinTaskbarProgress::WinTaskbarProgress() :
	_taskbar(nullptr),
	_window(nullptr)
{
}

WinTaskbarProgress::~WinTaskbarProgress()
{
	if (_taskbar)
		_taskbar->Release();
}

int WinTaskbarProgress::normalizedValue(int value)
{
	if (value < 0)
		return 0;
	if (value > 100)
		return 100;
	return value;
}

void WinTaskbarProgress::attach(HWND window)
{
	_window = window;
	if (!_window || _taskbar)
		return;

	ITaskbarList3* taskbar = nullptr;
	if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr,
			CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))))
		return;

	if (FAILED(taskbar->HrInit())) {
		taskbar->Release();
		return;
	}

	_taskbar = taskbar;
}

void WinTaskbarProgress::setProgress(int value)
{
	if (!_taskbar || !_window)
		return;

	_taskbar->SetProgressState(_window, TBPF_NORMAL);
	_taskbar->SetProgressValue(_window, normalizedValue(value), 100);
}

} // namespace pv
