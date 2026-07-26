# PXTOOL User Manual

Version: v1.0.0  
Document version: English initial draft  
Date: 2026-06-11

## Revision History

| Date | Document Version | Description |
| --- | --- | --- |
| 2026-06-11 | English initial draft | English user manual based on the current PXTOOL feature set |

## Contents

1. Overview  
2. Main Window  
3. Common Operations  
4. Logic Analyzer  
5. Oscilloscope  
6. Data Acquisition  
7. Files and Sessions  
8. Display, Language, and Logs  
9. Keyboard Shortcuts  
10. Troubleshooting

## 1 Overview

### 1.1 Introduction

PXTOOL is a cross-platform desktop application for signal capture, waveform visualization, measurement, search, protocol decoding, and data export. It provides a Qt-based graphical interface for supported DreamSourceLab instruments, saved data files, and built-in demo data.

Main features include:

| Feature | Description |
| --- | --- |
| Device connection and capture control | Select hardware, demo devices, or files; configure sample rate, capture depth, and capture mode |
| Digital waveform analysis | View logic channels, configure triggers, search patterns, measure timing, decode protocols, and filter glitches |
| Analog/oscilloscope analysis | View analog waveforms, configure triggers, perform automatic and cursor measurements, use FFT, and create math channels |
| Data acquisition | View, measure, save, and export continuous or sampled analog data |
| File and session workflows | Open, save, and export data; load and store device profiles; restore sessions; capture screenshots |
| Interface configuration | Switch dark/light themes, languages, display options, keyboard shortcuts, and log settings |

### 1.2 Work Modes

PXTOOL provides different work modes depending on the connected device or opened data file.

| Mode | Main Use |
| --- | --- |
| Logic Analyzer | Capture and analyze digital signals, with triggering, pattern search, protocol decoding, and glitch filtering |
| Oscilloscope | Capture and observe analog waveforms, with triggering, automatic measurements, FFT, and math functions |
| Data Acquisition | Capture slower or continuous analog data for trend observation, cursor measurement, and export |

### 1.3 Device Sources

The Device selector shows the currently available sources.

| Source | Description |
| --- | --- |
| Hardware device | A real acquisition device connected over USB |
| Demo device | Built-in demo source for learning the interface without hardware |
| File device | A saved data file opened and used as the current data source |

High sample rates and large capture depths require more transfer and storage resources. If capture becomes unstable, reduce the sample rate, enable fewer channels, or shorten the capture depth.

## 2 Main Window

### 2.1 Window Layout

The PXTOOL main window contains the following areas:

| Area | Purpose |
| --- | --- |
| Top menu bar | File, Window, Help, themes, language, display options, and related commands |
| Capture toolbar | Device selection, sample rate, buffer/capture depth, capture mode, Start/Stop, and single capture |
| Waveform view | Displays logic, analog, decoded, FFT, or math waveforms and results |
| Right sidebar | Trigger, Decode, Measure, Search, Device Options, Glitch Filter, and Log panels |
| Status area | Shows trigger time, capture status, capture progress, sample count, and measurement information |

### 2.2 Capture Toolbar

The capture toolbar contains the main settings used before starting acquisition.

| Control | Description |
| --- | --- |
| Trigger settings button | Shows or hides the trigger panel in the right sidebar |
| Device | Selects a hardware, demo, or file device |
| Sample Rate | Sets the sampling rate |
| Buffer | Sets capture depth, capture duration, or buffer size depending on the device mode |
| Mode | Selects Single, Repetitive, or Loop capture |
| Start/Stop | Starts or stops acquisition |
| Instant/Single | Runs an instant or single capture; the button label changes with the current mode |

Available sample rates, buffer sizes, and capture modes depend on the selected device. Some settings cannot be changed while capture is running.

### 2.3 Right Sidebar

The right sidebar provides analysis and configuration panels.

| Tab | Applies To | Purpose |
| --- | --- | --- |
| Trigger | Logic Analyzer, Oscilloscope | Configure trigger position, trigger type, and trigger conditions |
| Decode | Logic Analyzer | Add, configure, view, search, and export protocol decoding results |
| Measure | Logic Analyzer, Oscilloscope, Data Acquisition | Mouse measurement, cursor distance, edge statistics, and manual measurement |
| Search | Logic Analyzer | Search digital waveforms by per-channel pattern |
| Options | All modes | Configure device mode, channels, calibration, disk cache, and related device settings |
| Filter | Logic Analyzer | Apply polarity inversion and glitch filtering to captured digital data |
| Log | All modes | View, filter, search, and clear runtime logs |

### 2.4 Function Toolbar

The function toolbar changes according to the current work mode.

| Button | Logic Analyzer | Oscilloscope | Data Acquisition |
| --- | --- | --- | --- |
| Trigger | Available | Available | Hidden |
| Decode | Available | Hidden | Hidden |
| Measure | Available | Available | Available |
| Search | Available | Hidden | Hidden |
| Function | Hidden | Available, includes FFT and Math | Hidden |
| Display | Available | Available | Available |

## 3 Common Operations

### 3.1 Connecting a Device

1. Connect the acquisition device to the computer.
2. Select the target device from the Device list.
3. Select a work mode, such as Logic Analyzer, Oscilloscope, or Data Acquisition.
4. Open the Options panel and verify channel, mode, and device settings.
5. Set Sample Rate, Buffer, and capture mode.
6. Configure trigger, measurement, or protocol decoding as needed.
7. Click Start to begin acquisition.

### 3.2 Using a Demo Device

When no hardware is available, select a demo device to try the software. Demo data is useful for learning waveform navigation, measurement, search, and protocol decoding workflows. Demo data does not represent a real hardware input.

### 3.3 Opening a Data File

Use File / Open to open a saved data file. Once loaded, PXTOOL treats the file as the current data source, so you can view waveforms, measure, search, inspect decoding results, or export data.

### 3.4 Waveform Navigation

The waveform view supports common navigation operations:

| Operation | Description |
| --- | --- |
| Pan left/right | View different time ranges in the captured data |
| Zoom | Zoom in for detail or zoom out for context |
| Fit view | Fit the captured data into a suitable visible range |
| Jump to zero | Return to time zero or the trigger area |
| Cursor jump | Jump to a selected cursor from the measurement panel or cursor list |
| Result jump | Click a decoding or search result to jump to the corresponding waveform position |

### 3.5 Channel Display Adjustment

Channel display can be adjusted from the channel header area. Available options vary by signal type.

| Operation | Description |
| --- | --- |
| Enable/disable channel | Toggle a channel in the Options panel or through channel controls |
| Change color | Open the channel color selector and choose a new color |
| Rename channel | Assign a custom name to supported channels |
| Adjust height | Reset one row, reset all rows, or set Auto, 1X, 2X, 4X, or 8X height |
| Move channel | Drag a channel to change the display order |

### 3.6 Capture Modes

| Mode | Description |
| --- | --- |
| Single | Runs one capture and stops when complete |
| Repetitive | Repeats capture continuously, useful for observing stable signals |
| Loop | Captures in a loop for long observation; some post-processing features may be unavailable |

### 3.7 Buffer and Disk Cache

Buffer controls how much data is retained for one capture. PXTOOL also supports disk cache for large-depth captures.

| Setting | Description |
| --- | --- |
| Enable disk cache | Uses disk storage to extend available capture depth |
| RAM hot window | Sets the recent data window kept in memory |
| Disk total depth | Sets the total disk cache depth, including an unlimited option |
| Cache directory | Selects the cache directory |

Disk cache is useful for long or large-depth captures. Choose a cache directory with enough free space and good write performance.

## 4 Logic Analyzer

### 4.1 Hardware Connection

Logic Analyzer mode is used for digital signal capture. Before connecting, verify that:

1. The device is connected over USB.
2. Probe ground and the target system share a common ground.
3. The logic threshold matches the target signal level.
4. The required channels are enabled.
5. The sample rate is several times higher than the target signal frequency; protocol analysis usually needs additional margin.

### 4.2 Device Options

Open the Options panel to configure logic analyzer settings. Available items depend on the device model and current mode.

Common options include:

| Option | Description |
| --- | --- |
| Operation Mode | Selects the device operating mode, such as normal capture, stream capture, or other supported modes |
| Channel Mode | Selects the channel grouping mode; maximum channel count and maximum sample rate may not be available at the same time |
| Enable All / Disable All | Enables or disables all channels quickly |
| Channel Enable | Enables or disables individual channels |
| Threshold Voltage | Sets the digital logic threshold |
| Filter Targets | Selects targets for hardware-side or device-side filtering when supported |
| Max Height | Sets the maximum waveform display height |
| RLE Compress | Enables run-length compression for long captures with infrequent signal changes |
| External Clock | Uses an external clock as the sampling clock |
| Clock Negedge | Samples on the falling edge of the external clock |
| Stream Buffer | Sets the streaming capture buffer |
| Disk Cache | Enables disk cache for large-depth captures |

In Stream mode, some trigger and post-processing features may be limited. PXTOOL disables unavailable options automatically.

### 4.3 Sample Rate and Capture Depth

Sample Rate controls sampling frequency. Buffer controls capture depth or duration. A higher sample rate provides better time resolution, while a larger capture depth provides a longer observation window.

Recommended adjustments:

| Scenario | Recommendation |
| --- | --- |
| Need to inspect edges or narrow pulses | Increase the sample rate |
| Need to analyze long communication sequences | Increase Buffer or enable disk cache |
| Capture bandwidth is insufficient | Lower the sample rate, enable fewer channels, or shorten capture depth |
| Memory is insufficient | Reduce Buffer or enable disk cache |

### 4.4 Trigger

Logic Analyzer mode provides simple and advanced triggering.

#### 4.4.1 Trigger Position

Trigger Position is expressed as a percentage of the captured data. A smaller value keeps less pre-trigger data, while a larger value keeps more pre-trigger data. In Stream mode, trigger position may be restricted.

#### 4.4.2 Simple Trigger

Simple Trigger is suitable for quickly setting basic trigger conditions. You can set level or edge conditions on channels to capture a target event.

Trigger symbols:

| Symbol | Meaning |
| --- | --- |
| X | Don't care |
| 0 | Low level |
| 1 | High level |
| R | Rising edge |
| F | Falling edge |
| C | Rising or falling edge |

#### 4.4.3 Advanced Trigger

Advanced Trigger supports multi-stage trigger and serial trigger. Availability depends on device capability. Advanced Trigger is unavailable in Stream mode.

Advanced trigger types include:

| Type | Description |
| --- | --- |
| Stage Trigger | Multi-stage triggering with channel conditions, inversion, counters, contiguous conditions, and logical relation settings |
| Serial Trigger | Trigger by start flag, stop flag, clock flag, data channel, and data value |
| Hex Input | Enters serial trigger data in hexadecimal format |

### 4.5 Capture

After configuration, click Start to begin acquisition. During capture, the status area shows states such as waiting for trigger, triggered, capture progress, or complete.

| Status | Description |
| --- | --- |
| Waiting for Trigger | Acquisition has started and PXTOOL is waiting for the trigger condition |
| Triggered | The trigger condition has been met and capture continues for remaining data |
| Capturing | Data is being captured |
| Samples Captured | Capture is complete and the number of captured samples is shown |

If Data Overflow appears, the current acquisition settings exceed the device or transfer capability. Lower the sample rate, enable fewer channels, or shorten the capture depth.

### 4.6 Search

The Search panel searches logic waveforms for a specified per-channel pattern.

Workflow:

1. Open the Search panel.
2. Expand Edit per-channel.
3. Enter search conditions for each channel.
4. Click Search All to generate the result list.
5. Use Previous/Next or click a row in the result table to jump.

The result table contains index, time, and sample position. Search symbols are the same as trigger symbols: X, 0, 1, R, F, and C.

### 4.7 Measurement

The Measure panel provides the following measurement methods:

| Measurement | Description |
| --- | --- |
| Mouse measurement | Shows width, period, frequency, and duty cycle while moving or hovering the mouse |
| Enable floating measurement | Enables or disables the floating measurement tooltip |
| Cursor Distance | Adds a pair of cursors to measure time difference and sample difference |
| Edges | Counts rising edges, falling edges, and total edges in a selected channel range |
| Cursors | Manages created cursors and jumps to their positions |

### 4.8 Cursors

You can add X cursors or Y cursors in the waveform view. Logic Analyzer mode mainly uses X cursors for time measurements.

Common operations:

| Operation | Description |
| --- | --- |
| Add cursor | Add from the Measure panel or the waveform context menu |
| Move cursor | Drag a cursor to the target position |
| Jump to cursor | Use the measurement panel to jump to a cursor |
| Delete cursor | Use the delete button in the measurement row |

### 4.9 Protocol Decoder

The Decode panel adds and manages protocol decoders. PXTOOL integrates sigrok decoders and supports selected C decoders. Protocols with a C implementation show C and Py engine entries.

#### 4.9.1 Adding a Decoder

1. Open the Decode panel.
2. Use the search field to find a protocol.
3. Select Both, C, or Py to filter decoder sources.
4. Click the add button to add the protocol.
5. Bind channels and configure protocol options in the decoder settings.

If required channels are not specified, or if a decoder is bound to a disabled channel, the decoding trace shows an error.

#### 4.9.2 Stacked Decoding

Some protocols can decode based on another decoder's output. Add the upper-layer decoder, select the base decoder in its settings, and configure the required options.

#### 4.9.3 Protocol List Viewer

Protocol List Viewer displays decoded annotations in a table.

| Operation | Description |
| --- | --- |
| Configure displayed content | Select the protocols or fields to show |
| Search annotations | Enter a keyword and view the number of matches |
| Previous/Next | Navigate between matching results |
| Synchronized navigation | Select a table row to jump to the corresponding waveform position |
| Export results | Export the protocol list as CSV or TXT |

#### 4.9.4 Removing Decoders

You can remove a single decoder or remove all decoders at once. When channels are disabled, PXTOOL removes decoders that depend on those disabled channels to avoid invalid configurations.

### 4.10 Glitch Filter

The Filter panel applies post-processing to captured logic data.

| Function | Description |
| --- | --- |
| Invert | Inverts the entire channel polarity |
| Enable | Enables glitch removal for the channel |
| Threshold | Sets the maximum glitch pulse width in samples |
| Mode | Selects Both, High only, or Low only to limit which short pulses are removed |
| Apply | Applies the current settings to the captured data |
| Clear | Restores the original data |

Glitch filtering only applies to existing logic capture data. It cannot be applied while capture is running, and Loop capture results are not supported. After applying, the status area shows the number of processed regions or indicates that no glitches were found.

### 4.11 Logic Analyzer File Operations

Logic mode supports:

| Operation | Description |
| --- | --- |
| Save | Saves captured data as a DSView data file |
| Open | Opens a DSView data file |
| Export | Exports data to supported external formats |
| Capture | Saves a screenshot of the current interface |
| Config Load/Store | Loads or stores a device configuration file |
| Default | Loads the default configuration for the current device and mode |

## 5 Oscilloscope

### 5.1 Hardware Connection

Oscilloscope mode is used to observe analog voltage waveforms. Before connecting, verify that:

1. Probe grounding is reliable.
2. Input voltage does not exceed the device's allowed range.
3. Probe factor, coupling, and vertical range are set correctly.
4. For accurate measurements, run auto calibration or manual calibration first.

### 5.2 Device Options

The Options panel configures oscilloscope device settings. Available items depend on the device model.

Common options include:

| Option | Description |
| --- | --- |
| Operation Mode | Sets the oscilloscope operating mode |
| Bandwidth Limit | Enables or disables bandwidth limiting |
| Channel Enable | Enables or disables analog channels |
| Coupling | Selects GND, DC, or AC coupling |
| V/div | Sets the vertical scale |
| Offset | Adjusts vertical offset |
| Probe Factor | Sets the probe factor |
| Auto Calibration | Performs automatic calibration; do not connect probes during calibration |
| Manual Calibration | Manually adjusts gain, offset, and compensation settings |

### 5.3 Calibration

#### 5.3.1 Auto Calibration

Auto calibration corrects zero level and channel offset. Disconnect probes or ensure no signal is connected before running calibration. PXTOOL saves the calibration result after completion.

#### 5.3.2 Manual Calibration

Manual calibration is intended for advanced adjustment or maintenance. It can adjust channel gain, offset, and compensation values. Record the original state before changing these settings to avoid affecting later measurements.

### 5.4 Capture

Oscilloscope mode uses Sample Rate, Buffer, or horizontal timebase settings to control acquisition and display. Run/Stop starts or stops continuous capture. Single performs a one-shot capture.

| Operation | Description |
| --- | --- |
| Run/Stop | Starts or stops oscilloscope capture |
| Single | Performs a single triggered capture |
| Horizontal Resolution | Sets time resolution or horizontal timebase |
| Channel Customization | Sets channel color, visibility, vertical position, and scale |

### 5.5 Oscilloscope Trigger

The Trigger panel shows oscilloscope trigger options in Oscilloscope mode.

| Option | Description |
| --- | --- |
| Trigger Position | Sets the horizontal trigger position in the screen or captured data |
| Hold Off Time | Sets trigger holdoff time to avoid repeated triggering |
| Noise Sensitivity | Sets trigger noise sensitivity |
| Trigger Sources | Selects the trigger source, such as Auto, Channel 0, Channel 1, Channel 0 && 1, or Channel 0 \| 1 |
| Trigger Types | Selects Rising Edge or Falling Edge |

Disabled channels cannot be used as trigger sources.

### 5.6 Automatic Measurements

Oscilloscope mode supports multiple automatic measurement items. Open Measurements to select measurement items for a channel and display them in the status area.

| Item | Meaning |
| --- | --- |
| Freq | Frequency |
| Period | Period |
| +Duty / -Duty | Positive duty cycle / negative duty cycle |
| +Count | Positive pulse count |
| Rise / Fall | Rise time / fall time |
| +Width / -Width | Positive pulse width / negative pulse width |
| BrstW | Burst width |
| Ampl | Amplitude |
| High / Low | High level / low level |
| RMS | Root mean square |
| Mean | Mean value |
| PK-PK | Peak-to-peak value |
| Max / Min | Maximum / minimum |
| +Over / -Over | Positive overshoot / negative overshoot |

### 5.7 Cursor and Manual Measurements

Oscilloscope mode supports X cursors and Y cursors.

| Cursor | Purpose |
| --- | --- |
| X cursor | Measures time difference, frequency, period, and other horizontal values |
| Y cursor | Measures voltage difference, amplitude, and other vertical values |

Add cursors from the waveform context menu or manage them from the Measure panel.

### 5.8 FFT Spectrum Analysis

FFT in the Function menu performs spectrum analysis on oscilloscope channels.

FFT Options include:

| Option | Description |
| --- | --- |
| FFT Enable | Enables or disables FFT |
| FFT Length | Sets FFT point count |
| Sample Interval | Sets the sampling interval used for FFT |
| FFT Source | Selects the input channel |
| FFT Window | Selects Rectangle, Hann, Hamming, Blackman, or Flat_top window |
| DC Ignored | Ignores the DC component |
| Y-axis Mode | Sets the spectrum Y-axis display mode |
| DBV Range | Sets the dBV display range |

The spectrum view supports zoom and pan. Window choice affects leakage and amplitude accuracy: Rectangle provides high frequency resolution with more leakage, Flat_top is better for amplitude accuracy, and Hann, Hamming, and Blackman help reduce spectral leakage.

### 5.9 Math Channel

Math in the Function menu creates a math channel.

Available operations:

| Operation | Description |
| --- | --- |
| Add | Adds two channels |
| Subtract | Subtracts the second source from the first source |
| Multiply | Multiplies two channels |
| Divide | Divides the first source by the second source |

In Math Options, enable the math channel and select 1st Source and 2nd Source.

### 5.10 Oscilloscope File Operations

Oscilloscope mode supports saving data, opening data, exporting CSV, saving screenshots, and loading or storing device configurations. Saving or exporting may be limited when the current session contains multiple data types.

## 6 Data Acquisition

### 6.1 Hardware Connection

Data Acquisition mode is suitable for observing analog values over time. Before connecting, verify range, grounding, and input limits.

### 6.2 Device Options

The Options panel may include:

| Option | Description |
| --- | --- |
| Operation Mode | Sets the data acquisition operating mode |
| Bandwidth Limit | Enables or disables bandwidth limiting |
| Channel Enable | Enables or disables channels |
| Coupling / Offset / Range | Sets channel input mode, offset, and range when supported by the device |

### 6.3 Capture

After selecting the device, sample rate, and Buffer, click Start to begin acquisition and Stop to stop acquisition. Data Acquisition mode usually does not show logic trigger or protocol decoding features.

### 6.4 Measurement

Data Acquisition mode supports cursor and manual measurements in the Measure panel. Use X cursors to measure time intervals and Y cursors to measure value differences.

### 6.5 File Operations

Data Acquisition mode supports save, open, export, and screenshot operations. Export formats depend on the current device and data type; CSV is the common format.

## 7 Files and Sessions

### 7.1 File Menu

| Menu Item | Description |
| --- | --- |
| Config / Load | Loads a device configuration file |
| Config / Store | Stores the current device configuration |
| Config / Default | Loads the default device configuration |
| Disk Cache Settings | Opens disk cache settings |
| Open | Opens a saved data file |
| Save | Saves current captured data |
| Export | Exports current data |
| Capture | Saves a screenshot of the current interface |
| Exit | Exits the application |

### 7.2 Data Files

PXTOOL can save and open DSView data files. A data file contains captured data, metadata, decoder configuration, and session information for later analysis.

Save limitations:

| Case | Description |
| --- | --- |
| No captured data | Save is unavailable |
| Mixed data types | Saving as a single data file is currently unsupported |
| No write permission | Save fails; choose another directory |

### 7.3 Configuration Files

Configuration files store device mode, channel settings, and related parameters. They are not the same as captured data files and are mainly used to restore device settings quickly.

Common uses:

| Use | Description |
| --- | --- |
| Save a common channel layout | Restore the layout quickly when using the same type of device |
| Save sampling and trigger preferences | Reduce repetitive setup |
| Restore defaults | Use Default to load the default profile for the current device mode |

### 7.4 Export

Export writes captured data to formats readable by external tools. In Logic Analyzer mode, available formats depend on the internal output modules. Oscilloscope and Data Acquisition modes usually export CSV.

Protocol decoding results can be exported separately as CSV or TXT from the Decode panel.

### 7.5 Screenshot

Capture saves a screenshot of the current interface. PNG and JPEG are supported. Screenshots are useful for recording waveform states, measurement results, or debugging context.

## 8 Display, Language, and Logs

### 8.1 Themes

PXTOOL supports Dark, Light, Atom One Dark, Ayu Light, Dark Colored Cards, and Light Colored Cards themes. Switch themes from the Window menu or Display / Themes.

### 8.2 Language

Help / Language supports:

| Language | Description |
| --- | --- |
| English | English |
| 简体中文 | Simplified Chinese |
| 繁體中文 | Traditional Chinese |

After switching language, interface text updates accordingly. Some status messages or device-provided strings may still depend on the device or system language.

### 8.3 Display Options

Display Options configures interface behavior.

| Group | Option | Description |
| --- | --- | --- |
| Logic | Quick scroll | Enables quick scrolling |
| Logic | Used abort data | Uses data retained after an aborted capture |
| Logic | Auto scroll latest | Automatically scrolls to the latest data |
| Scope | Trig pos in middle | Displays the oscilloscope trigger position in the middle |
| UI | Profile in bar | Shows profile-related information in the toolbar |
| UI | Font size | Sets interface font size |

### 8.4 Keyboard Shortcuts

Window / Keyboard Shortcuts opens the shortcut editor. Double-click a shortcut cell to edit it. Leave a shortcut empty to disable it. If a shortcut conflicts with another action, PXTOOL displays a warning.

### 8.5 Log Options

Help / Log Options configures logging behavior.

| Option | Description |
| --- | --- |
| Log Level | Sets log level from 0 to 5 |
| Save To File | Saves logs to a local file |
| Append mode | Appends to the log file instead of overwriting it |
| Open | Opens the log file |
| Clear | Clears the log file or log content |

### 8.6 Log Panel

The Log panel in the right sidebar can display runtime logs and supports:

| Function | Description |
| --- | --- |
| Level | Filters by log level |
| Search | Searches logs by keyword |
| Exact | Uses exact matching |
| Previous / Next | Navigates between search results |
| Clear | Clears currently displayed logs |

## 9 Keyboard Shortcuts

PXTOOL provides the following default shortcuts. They can be changed in Keyboard Shortcuts.

| Action | Default Shortcut |
| --- | --- |
| Start Collecting | F1 |
| Stop Collecting | F2 |
| Prev Cursor | F3 |
| Next Cursor | F4 |
| Previous Tab | F5 |
| Next Tab | F6 |
| Jump to Zero | F7 |
| Zoom In | F8 |
| Zoom Out | F9 |
| Zoom Fit | F10 |
| Add Cursor | Ctrl+H |
| Measure Panel | Ctrl+G |
| Device Config | Ctrl+1 |
| Protocol Decode | Ctrl+2 |
| Label Measurement | Ctrl+3 |
| Data Search | Ctrl+F |
| Save | Ctrl+S |
| Save As | Ctrl+Shift+S |
| Export | Ctrl+E |
| Close Session | Ctrl+W |

## 10 Troubleshooting

### 10.1 Device Not Found

Check that the device is connected, the USB cable is working, and required drivers or system permissions are configured. If the device was just plugged in, wait briefly or reconnect it. On Linux, make sure the current user has permission to access USB devices.

### 10.2 Reconnect Device Prompt

If hardware configuration refresh fails, firmware loading fails, or USB communication errors occur, PXTOOL may ask you to reconnect the device. Unplug the device, plug it in again, and select it again.

### 10.3 Insufficient Memory

If PXTOOL reports insufficient memory, reduce capture depth, enable fewer channels, disable unnecessary features, or enable disk cache.

### 10.4 Capture Bandwidth Insufficient

If Data Overflow appears, the current sampling settings exceed the device or transfer capability. Recommended actions:

1. Lower the sample rate.
2. Enable fewer channels.
3. Shorten capture depth.
4. Disable analysis features that are not needed for the current task.

### 10.5 Cannot Save or Export

Common causes:

| Cause | Action |
| --- | --- |
| No captured data | Capture data first or open a data file |
| Empty file path | Choose a save location again |
| No write permission | Choose another directory or check system permissions |
| Mixed data types | Save or export one data type at a time |
| Unsupported export format | Select a format supported by the current mode |

### 10.6 Protocol Decoder Has No Output

Check that:

1. Decoder channels are bound correctly.
2. Bound channels are enabled.
3. The sample rate is high enough.
4. Protocol parameters match the real signal.
5. The waveform contains complete protocol frames.
6. If the protocol provides both C and Py engines, try switching the decoding engine.

### 10.7 Glitch Filter Has No Effect

Confirm that the current data is logic analyzer data, capture has stopped, and the data is not from Loop capture. If the status says No glitches found, no short pulses matched the current threshold and mode.

### 10.8 FFT Not Displayed

Confirm that an oscilloscope channel is enabled and has captured data, FFT Enable is on, FFT Source selects a valid channel, and FFT Length does not exceed the available sample length.

## Appendix A Startup Behavior

PXTOOL can be started from the application icon or by opening one data file at startup. Only one file can be opened automatically per launch. Advanced users can also set log level, enable log saving, view version information, or show help at startup.

## Appendix B File Type Overview

| Type | Purpose |
| --- | --- |
| DSView data file | Stores captured data, metadata, decoder configuration, and session information |
| DSView configuration file | Stores device mode, channel, and parameter configuration |
| CSV | Exports data or protocol lists for spreadsheet and script tools |
| TXT | Exports protocol decoding text results |
| PNG / JPEG | Saves interface screenshots |
