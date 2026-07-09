#pragma once

// printer.h — motion constants and CMD namespace for the test rig.
//
// Adapted from CREATE_LAB_Binder_Jet_Printer's printer.h.
// Stripped to the two axes this rig actually has (X and D) and the
// peripheral bits wired up on this machine.
//
// ── Axis mapping ────────────────────────────────────────────────────────────
//   Axis::X  →  Galil axis A (X carriage, FUYU FSL30 + Nema 14 stepper)
//   Axis::D  →  Galil axis D (reservoir height stage, same hardware)
//
// ── Future axis ─────────────────────────────────────────────────────────────
//   When the X carriage is swapped for the longer stage used on the custom
//   printer, only X_CNTS_PER_MM needs updating (and program.dmc limits).
//   Everything else — jog speed defaults, the CMD namespace, widget code —
//   stays the same.

#ifndef PRINTER_H
#define PRINTER_H

#include <string>
#include <sstream>
#include <stdexcept>
#include <cmath>

// ── Axes ────────────────────────────────────────────────────────────────────

enum class Axis { X, D };

// ── Counts-per-mm ───────────────────────────────────────────────────────────
// Both stages: 200 steps/rev × 256 microsteps × (1 rev / 2 mm lead) = 25 600 cnt/mm.
// Update the relevant constant if a different stage is fitted to either axis.

#define X_CNTS_PER_MM 25600.0
#define D_CNTS_PER_MM 25600.0   // same hardware as X for now

// ── Digital output bit numbers ──────────────────────────────────────────────
// Set the active bit in GalilController::quickPurge() and setSolenoid().

#define SOLENOID_BIT 22

// JetForge (Added Scientific) trigger bits — verified against custom printer
// wiring; recheck if the test-rig Galil extension I/O is wired differently.
#define MJ_START_BIT  23   // pin 18 on the custom printer
#define MJ_DIR_BIT    22   // pin 32 on the custom printer

// ── CMD namespace ────────────────────────────────────────────────────────────
// Builds Galil DMC program strings.  Each function returns a single line in
// the format  "GCmd,<galil_command>\n"  which cmd_buf_to_dmc() converts to
// a raw DMC program for GProgramDownload().
//
// Usage:
//   std::stringstream s;
//   s << CMD::set_speed(Axis::X, 50.0);
//   s << CMD::position_absolute(Axis::X, 10.0);
//   s << CMD::begin_motion(Axis::X);
//   s << CMD::after_motion(Axis::X);
//   m_galil->downloadAndRun(CMD::cmd_buf_to_dmc(s));

namespace CMD {

namespace detail {

    inline std::string axis_string(Axis axis)
    {
        switch (axis)
        {
        case Axis::X:  return {"A"};   // Galil axis A = X carriage
        case Axis::D:  return {"D"};   // Galil axis D = reservoir
        default:
            throw std::invalid_argument("invalid axis");
        }
    }

    inline long mm2cnts(double mm, Axis axis)
    {
        switch (axis)
        {
        case Axis::X:  return static_cast<long>(mm * X_CNTS_PER_MM);
        case Axis::D:  return static_cast<long>(mm * D_CNTS_PER_MM);
        default:
            throw std::invalid_argument("invalid axis");
        }
    }

    // The "GCmd," prefix is used by cmd_buf_to_dmc() to identify command lines.
    inline std::string GCmd() { return "GCmd,"; }
    inline std::string GCmd(std::string_view command)
    {
        return GCmd() + command.data() + "\n";
    }

} // namespace detail


// ── Motion parameter setters ─────────────────────────────────────────────────

inline std::string set_speed(Axis axis, double mm_per_s)
{
    return detail::GCmd("SP" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm_per_s, axis)));
}

inline std::string set_accleration(Axis axis, double mm_per_s2)   // spelling matches original
{
    return detail::GCmd("AC" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm_per_s2, axis)));
}

inline std::string set_deceleration(Axis axis, double mm_per_s2)
{
    return detail::GCmd("DC" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm_per_s2, axis)));
}

inline std::string set_jog(Axis axis, double mm_per_s)
{
    return detail::GCmd("JG" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm_per_s, axis)));
}

// ── Motion commands ──────────────────────────────────────────────────────────

inline std::string position_relative(Axis axis, double mm)
{
    return detail::GCmd("PR" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm, axis)));
}

inline std::string position_absolute(Axis axis, double mm)
{
    return detail::GCmd("PA" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm, axis)));
}

inline std::string begin_motion(Axis axis)
{
    return detail::GCmd("BG" + detail::axis_string(axis));
}

inline std::string stop_motion(Axis axis)
{
    return detail::GCmd("ST" + detail::axis_string(axis));
}

// after_motion: Galil AM command — blocks *within the DMC program* until
// the axis finishes moving.  Does NOT block the host.
inline std::string after_motion(Axis axis)
{
    return detail::GCmd("AM" + detail::axis_string(axis));
}

// find_edge: jog toward reverse limit until it trips, used for homing.
// Must have called set_jog() with a negative speed beforehand.
inline std::string find_edge(Axis axis)
{
    return detail::GCmd("FE" + detail::axis_string(axis));
}

inline std::string motor_on(Axis axis)
{
    return detail::GCmd("SH" + detail::axis_string(axis));
}

inline std::string motor_off(Axis axis)
{
    return detail::GCmd("MO" + detail::axis_string(axis));
}

inline std::string define_position(Axis axis, double mm = 0.0)
{
    return detail::GCmd("DP" + detail::axis_string(axis) + "="
                        + std::to_string(detail::mm2cnts(mm, axis)));
}

// ── Timing ───────────────────────────────────────────────────────────────────

inline std::string sleep(int milliseconds)
{
    return detail::GCmd("WT " + std::to_string(milliseconds));
}

// ── Digital I/O ──────────────────────────────────────────────────────────────

inline std::string set_bit(int bit)
{
    return detail::GCmd("SB " + std::to_string(bit));
}

inline std::string clear_bit(int bit)
{
    return detail::GCmd("CB " + std::to_string(bit));
}

// ── Solenoid / purge helpers ─────────────────────────────────────────────────

// Quick-purge pulse: arms the solenoid for pulseTime_ms milliseconds.
inline std::string quick_purge(int pulseTime_ms)
{
    std::stringstream s;
    s << set_bit(SOLENOID_BIT);
    s << sleep(pulseTime_ms);
    s << clear_bit(SOLENOID_BIT);
    return s.str();
}

// ── JetForge (Added Scientific) trigger bits ─────────────────────────────────

inline std::string start_MJ_print()    { return set_bit(MJ_START_BIT);  }
inline std::string disable_MJ_start()  { return clear_bit(MJ_START_BIT); }
inline std::string start_MJ_dir()      { return set_bit(MJ_DIR_BIT);    }
inline std::string disable_MJ_dir()    { return clear_bit(MJ_DIR_BIT);  }

// ── Messages ─────────────────────────────────────────────────────────────────

inline std::string display_message(const std::string &message)
{
    return detail::GCmd("MG \"" + message + "\"");
}

// ── Conversion utility ───────────────────────────────────────────────────────

// Converts a CMD stringstream (lines of the form "GCmd,<command>\n")
// into a raw DMC program string suitable for GProgramDownload().
std::string cmd_buf_to_dmc(const std::stringstream &s);

} // namespace CMD

#endif // PRINTER_H
