# PXTOOL MSO Mode Design

## Goal

Add a Mixed Signal Oscilloscope (MSO) mode that acquires and displays logic and analog channels in one time-domain view. DSO channels are excluded.

## Scope

`MSO = 3` is offered only when a device exposes both `SR_CHANNEL_LOGIC` and `SR_CHANNEL_ANALOG` and can emit both `SR_DF_LOGIC` and `SR_DF_ANALOG` during one acquisition.

Logic-only PXLogic hardware must not advertise MSO. The native demo driver is the first supported producer; hardware drivers can opt in only after proving simultaneous digital and analog transport.

## Architecture

libsigrok gains the mode value and a mode-list descriptor. The native demo retains both channel banks in MSO, schedules its logic and analog feeds, and sends one `SR_DF_END` when both complete.

PXTOOL derives the selector entry from channel capabilities. `SigSession` centralizes the policy: MSO enables and creates LogicSignal plus AnalogSignal, preserves logic decoding, and takes its horizontal sample count from LogicSnapshot. The view treats MSO as logic-capable for logic tools and preserves AnalogSignal rendering; it never enables DSO controls.

## Rules

| Mode | Channel types | Packets | DSO controls |
| --- | --- | --- | --- |
| LOGIC | LOGIC | `SR_DF_LOGIC` | No |
| ANALOG | ANALOG | `SR_DF_ANALOG` | No |
| DSO | DSO | `SR_DF_DSO` | Yes |
| MSO | LOGIC and ANALOG | `SR_DF_LOGIC` and `SR_DF_ANALOG` | No |

Switching to or from MSO clears capture and decoder state as a logic transition. The demo rejects an MSO start unless at least one logic and one analog channel are enabled. It delays end-of-capture until both feeds are complete.

## Compatibility And Verification

Existing mode values remain unchanged. Imported or file-backed sessions advertise MSO only for mixed channel lists. Tests cover mode enumeration, demo dual data feeds, PXTOOL mode policy, and mixed session persistence. Manual validation confirms synchronous cursors, a logic decoder on an MSO logic signal, and no DSO-only UI.
