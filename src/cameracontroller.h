#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

// Forward-declare IDS peak types so SDK headers stay out of this interface.
// Callers only need Qt and standard types.
namespace peak { namespace core {
class Device;
class DataStream;
class NodeMap;
class Buffer;
}}

// QImage is a Qt type — forward declare to avoid pulling in QtGui here.
// Include <QImage> in files that actually use the returned value.
class QImage;

// ---------------------------------------------------------------------------
// CameraController
//
// Plain C++ wrapper around the IDS peak genericSDK for the UI-3370CP-NIR-GL.
// Owns the device handle, data stream, and node map.  All SDK types are
// confined to the .cpp; nothing leaks through this interface.
//
// Thread safety: NOT thread-safe.  Call all methods from the same thread
// (typically a dedicated QThread via CameraWorker).
// ---------------------------------------------------------------------------
class CameraController
{
public:
    CameraController();
    ~CameraController();

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Initialize the IDS peak library, enumerate cameras, open the first
    /// one found, and allocate acquisition buffers.
    /// Returns true on success.  On failure, errorMessage() is set.
    bool open();

    /// Stop acquisition (if running), release buffers, and close the device.
    void close();

    /// True if a camera is currently open and ready.
    bool isOpen() const;

    /// Human-readable description of the last error.
    std::string errorMessage() const;

    // ------------------------------------------------------------------
    // Exposure  (node: ExposureTime, unit: microseconds)
    // ------------------------------------------------------------------
    void   setExposure(double microseconds);
    double getExposure() const;
    double getExposureMin() const;
    double getExposureMax() const;

    // ------------------------------------------------------------------
    // Gain  (node: Gain, dimensionless multiplier)
    // ------------------------------------------------------------------
    void   setGain(double value);
    double getGain() const;
    double getGainMin() const;
    double getGainMax() const;

    // ------------------------------------------------------------------
    // Pixel format  (node: PixelFormat)
    // Typical values for this sensor: "Mono8", "Mono10", "Mono12"
    // ------------------------------------------------------------------
    void                     setPixelFormat(const std::string& format);
    std::string              getPixelFormat() const;
    std::vector<std::string> availablePixelFormats() const;

    // ------------------------------------------------------------------
    // Region of interest
    // Nodes: OffsetX, OffsetY, Width, Height
    // Hardware step constraints (from datasheet):
    //   Width  step: 16 px (min width: 16)
    //   Height step:  2 px (min height: 2)
    //   OffsetX grid: 2 px
    //   OffsetY grid: 2 px
    // Values are clamped/aligned internally before writing to the camera.
    // ------------------------------------------------------------------
    void setROI(int offsetX, int offsetY, int width, int height);
    int  getOffsetX() const;
    int  getOffsetY() const;
    int  getWidth()   const;
    int  getHeight()  const;
    int  getSensorWidth()  const;   // full-sensor max (2048)
    int  getSensorHeight() const;   // full-sensor max (2048)

    // ------------------------------------------------------------------
    // Decimation  (nodes: DecimationHorizontal, DecimationVertical)
    // Supported factors: 2, 4, 6, 8  (or 1 = off, if supported)
    // Horizontal and vertical decimation are set independently so the
    // caller can choose asymmetric decimation; use setDecimation() to
    // set both at once.
    // ------------------------------------------------------------------
    void setDecimation(int factor);             // sets H and V together
    void setDecimationH(int factor);
    void setDecimationV(int factor);
    int  getDecimationH() const;
    int  getDecimationV() const;

    // ------------------------------------------------------------------
    // Trigger
    //
    // For stroboscopic use the typical setup is:
    //   TriggerSelector = "ExposureStart"
    //   TriggerMode     = "On"
    //   TriggerSource   = one of the lines below
    //   TriggerDivider  = N  (capture every Nth pulse)
    //   TriggerDelay    = microseconds (delay between pulse and exposure)
    //
    // Physical lines on the Hirose connector:
    //   "Line0" → Trigger input, optocoupled  (pins 4 / 7)
    //   "Line2" → GPIO 1, 3.3 V logic         (pin 3)
    //   "Line3" → GPIO 2, 3.3 V logic         (pin 6)
    //
    // Note: TriggerDivider is an IDS extension node (not base SFNC).
    // Verify it appears in IDS peak Cockpit at Expert/Guru visibility
    // for your firmware before relying on it.
    // ------------------------------------------------------------------
    void                     setTriggerSource(const std::string& line);
    std::string              getTriggerSource() const;
    std::vector<std::string> availableTriggerSources() const;

    void setTriggerDivider(int n);
    int  getTriggerDivider() const;

    void   setTriggerDelay(double microseconds);
    double getTriggerDelay() const;

    void setTriggerEnabled(bool enabled);

    // ------------------------------------------------------------------
    // Flash / strobe output
    //
    // For UI (uEye legacy) models the supported FlashReference modes are:
    //   "ExposureActive" — flash mirrors the exposure window exactly.
    //                      FlashStartDelay / FlashEndDelay shift the edges.
    //   "ExposureStart"  — flash is timed relative to exposure start.
    //                      Duration is set explicitly via FlashDuration.
    //                      FlashEndDelay is NOT available on UI models.
    //
    // Recommended for stroboscopic use: "ExposureStart", then set
    // FlashStartDelay and FlashDuration independently of exposure length.
    //
    // Physical flash output line:
    //   "Line1" → Flash output, optocoupled  (pins 2 / 5)
    //
    // Typical setup sequence (also reflected in configureFlashOutput()):
    //   LineSelector = "Line1"
    //   LineSource   = "FlashActive"
    //   FlashReference  = "ExposureStart"
    //   FlashStartDelay = <delay from exposure start to strobe on>  [µs]
    //   FlashDuration   = <how long to keep strobe on>              [µs]
    // ------------------------------------------------------------------
    void setFlashReference(const std::string& ref);   // "ExposureStart" or "ExposureActive"
    void setFlashStartDelay(double microseconds);
    void setFlashDuration(double microseconds);

    /// Convenience: configure Line1 as FlashActive output and apply all
    /// flash timing in one call.
    void configureFlashOutput(const std::string& flashReference,
                              double startDelayUs,
                              double durationUs);

    double getFlashStartDelay() const;
    double getFlashDuration()   const;

    // ------------------------------------------------------------------
    // Acquisition
    // ------------------------------------------------------------------

    /// Arm the camera for triggered or free-run acquisition.
    /// Call after all settings are configured.
    void startAcquisition();

    /// Stop acquisition and drain any pending buffers.
    void stopAcquisition();

    bool isAcquiring() const;

    /// Block until a frame arrives (or timeout_ms elapses) and return it
    /// as a QImage.  Returns a null QImage on timeout or error.
    /// Intended to be called in a loop from CameraWorker's thread.
    ///
    /// Include <QImage> in CameraWorker.cpp — not needed in this header.
    QImage acquireFrame(int timeout_ms = 2000);

private:
    // Pimpl keeps all IDS peak headers out of this header entirely.
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // CAMERACONTROLLER_H
