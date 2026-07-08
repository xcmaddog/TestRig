#include "cameracontroller.h"

// All IDS peak SDK headers are confined to this translation unit.
#include <peak/peak.hpp>
#include <peak_ipl/peak_ipl.hpp>

#include <QImage>

#include <stdexcept>
#include <algorithm>

// ---------------------------------------------------------------------------
// Internal helpers — not visible outside this file
// ---------------------------------------------------------------------------
namespace {

using NodeMap = peak::core::NodeMap;

/// Read/write a float node.
double getFloat(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::FloatNode>(name)->Value();
}

void setFloat(const std::shared_ptr<NodeMap>& nm,
              const std::string& name, double value)
{
    nm->FindNode<peak::core::nodes::FloatNode>(name)->SetValue(value);
}

double getFloatMin(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::FloatNode>(name)->Minimum();
}

double getFloatMax(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::FloatNode>(name)->Maximum();
}

/// Read/write an integer node.
int64_t getInt(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::IntegerNode>(name)->Value();
}

void setInt(const std::shared_ptr<NodeMap>& nm,
            const std::string& name, int64_t value)
{
    nm->FindNode<peak::core::nodes::IntegerNode>(name)->SetValue(value);
}

int64_t getIntMin(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::IntegerNode>(name)->Minimum();
}

int64_t getIntMax(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::IntegerNode>(name)->Maximum();
}

int64_t getIntInc(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::IntegerNode>(name)->Increment();
}

/// Read/write an enumeration node.
std::string getEnum(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    return nm->FindNode<peak::core::nodes::EnumerationNode>(name)
    ->CurrentEntry()->SymbolicValue();
}

void setEnum(const std::shared_ptr<NodeMap>& nm,
             const std::string& name, const std::string& entry)
{
    nm->FindNode<peak::core::nodes::EnumerationNode>(name)
    ->SetCurrentEntry(entry);
}

/// Return all accessible entries for an enumeration node.
std::vector<std::string> enumEntries(const std::shared_ptr<NodeMap>& nm,
                                     const std::string& name)
{
    auto all = nm->FindNode<peak::core::nodes::EnumerationNode>(name)->Entries();
    std::vector<std::string> out;
    for (const auto& e : all) {
        auto s = e->AccessStatus();
        if (s != peak::core::nodes::NodeAccessStatus::NotAvailable &&
            s != peak::core::nodes::NodeAccessStatus::NotImplemented)
        {
            out.push_back(e->SymbolicValue());
        }
    }
    return out;
}

/// Execute a command node.
void execCommand(const std::shared_ptr<NodeMap>& nm, const std::string& name)
{
    nm->FindNode<peak::core::nodes::CommandNode>(name)->Execute();
    nm->FindNode<peak::core::nodes::CommandNode>(name)->WaitUntilDone();
}

/// Align `value` to the nearest multiple of `step`, clamped to [lo, hi].
int64_t align(int64_t value, int64_t step, int64_t lo, int64_t hi)
{
    value = std::clamp(value, lo, hi);
    value = lo + ((value - lo) / step) * step;  // round down to grid
    return value;
}

} // anonymous namespace


// ---------------------------------------------------------------------------
// Pimpl struct — owns all SDK objects
// ---------------------------------------------------------------------------
struct CameraController::Impl
{
    // IDS peak library is reference-counted; Library::Initialize() /
    // Finalize() calls must be balanced.
    bool libraryInitialized = false;

    std::shared_ptr<peak::core::Device>     device;
    std::shared_ptr<peak::core::DataStream> dataStream;
    std::shared_ptr<peak::core::NodeMap>    nodeMap;   // remote device node map

    bool acquiring = false;
    std::string lastError;

    // Buffer count allocated for the data stream.
    static constexpr size_t BUFFER_COUNT = 4;
};


// ---------------------------------------------------------------------------
// CameraController — lifecycle
// ---------------------------------------------------------------------------

CameraController::CameraController()
    : m_impl(std::make_unique<Impl>())
{}

CameraController::~CameraController()
{
    close();
}

bool CameraController::open()
{
    try {
        peak::Library::Initialize();
    } catch (const peak::core::Exception& e) {
        m_impl->lastError = std::string("Initialize failed (peak): ") + e.what();
        return false;
    } catch (const std::exception& e) {
        m_impl->lastError = std::string("Initialize failed (std): ") + e.what();
        return false;
    } catch (...) {
        m_impl->lastError = "Initialize failed: unknown exception.";
        return false;
    }

    m_impl->libraryInitialized = true;

    try {
        auto& deviceManager = peak::DeviceManager::Instance();
        deviceManager.Update();

        if (deviceManager.Devices().empty()) {
            m_impl->lastError = "No IDS cameras found.";
            return false;
        }

        m_impl->device = deviceManager.Devices().front()->OpenDevice(
            peak::core::DeviceAccessType::Control);

        m_impl->nodeMap = m_impl->device->RemoteDevice()->NodeMaps().front();

        m_impl->dataStream = m_impl->device->DataStreams().front()->OpenDataStream();

        auto payloadSize = getInt(m_impl->nodeMap, "PayloadSize");
        for (size_t i = 0; i < Impl::BUFFER_COUNT; ++i) {
            auto buf = m_impl->dataStream->AllocAndAnnounceBuffer(
                static_cast<size_t>(payloadSize), nullptr);
            m_impl->dataStream->QueueBuffer(buf);
        }

        return true;

    } catch (const peak::core::Exception& e) {
        m_impl->lastError = std::string("Device open failed (peak): ") + e.what();
        return false;
    } catch (const std::exception& e) {
        m_impl->lastError = std::string("Device open failed (std): ") + e.what();
        return false;
    } catch (...) {
        m_impl->lastError = "Device open failed: unknown exception.";
        return false;
    }
}

void CameraController::close()
{
    if (m_impl->acquiring)
        stopAcquisition();

    try {
        if (m_impl->dataStream) {
            m_impl->dataStream->Flush(peak::core::DataStreamFlushMode::DiscardAll);
            for (auto& buf : m_impl->dataStream->AnnouncedBuffers())
                m_impl->dataStream->RevokeBuffer(buf);
            m_impl->dataStream.reset();
        }
        m_impl->device.reset();
    } catch (...) {}

    if (m_impl->libraryInitialized) {
        peak::Library::Close();
        m_impl->libraryInitialized = false;
    }
}

bool CameraController::isOpen() const
{
    return m_impl->device != nullptr;
}

std::string CameraController::errorMessage() const
{
    return m_impl->lastError;
}


// ---------------------------------------------------------------------------
// Exposure
// ---------------------------------------------------------------------------

void CameraController::setExposure(double microseconds)
{
    try {
        setFloat(m_impl->nodeMap, "ExposureTime", microseconds);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

double CameraController::getExposure() const
{
    return getFloat(m_impl->nodeMap, "ExposureTime");
}

double CameraController::getExposureMin() const
{
    return getFloatMin(m_impl->nodeMap, "ExposureTime");
}

double CameraController::getExposureMax() const
{
    return getFloatMax(m_impl->nodeMap, "ExposureTime");
}


// ---------------------------------------------------------------------------
// Gain
// ---------------------------------------------------------------------------

void CameraController::setGain(double value)
{
    try {
        setFloat(m_impl->nodeMap, "Gain", value);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

double CameraController::getGain() const
{
    return getFloat(m_impl->nodeMap, "Gain");
}

double CameraController::getGainMin() const
{
    return getFloatMin(m_impl->nodeMap, "Gain");
}

double CameraController::getGainMax() const
{
    return getFloatMax(m_impl->nodeMap, "Gain");
}


// ---------------------------------------------------------------------------
// Pixel format
// ---------------------------------------------------------------------------

void CameraController::setPixelFormat(const std::string& format)
{
    try {
        setEnum(m_impl->nodeMap, "PixelFormat", format);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

std::string CameraController::getPixelFormat() const
{
    return getEnum(m_impl->nodeMap, "PixelFormat");
}

std::vector<std::string> CameraController::availablePixelFormats() const
{
    return enumEntries(m_impl->nodeMap, "PixelFormat");
}


// ---------------------------------------------------------------------------
// ROI
// ---------------------------------------------------------------------------

void CameraController::setROI(int offsetX, int offsetY, int width, int height)
{
    try {
        // Must reset width/height before offsets to avoid range conflicts.
        // Order: Width → Height → OffsetX → OffsetY.
        // Fetch hardware constraints from the node map.
        auto wMax  = getIntMax(m_impl->nodeMap, "Width");
        auto wMin  = getIntMin(m_impl->nodeMap, "Width");
        auto wInc  = getIntInc(m_impl->nodeMap, "Width");
        auto hMax  = getIntMax(m_impl->nodeMap, "Height");
        auto hMin  = getIntMin(m_impl->nodeMap, "Height");
        auto hInc  = getIntInc(m_impl->nodeMap, "Height");
        auto oxInc = getIntInc(m_impl->nodeMap, "OffsetX");
        auto oyInc = getIntInc(m_impl->nodeMap, "OffsetY");

        int64_t w  = align(width,   wInc,  wMin, wMax);
        int64_t h  = align(height,  hInc,  hMin, hMax);
        int64_t ox = align(offsetX, oxInc, 0,    wMax - w);
        int64_t oy = align(offsetY, oyInc, 0,    hMax - h);

        setInt(m_impl->nodeMap, "Width",   w);
        setInt(m_impl->nodeMap, "Height",  h);
        setInt(m_impl->nodeMap, "OffsetX", ox);
        setInt(m_impl->nodeMap, "OffsetY", oy);

    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

int CameraController::getOffsetX()     const { return (int)getInt(m_impl->nodeMap, "OffsetX"); }
int CameraController::getOffsetY()     const { return (int)getInt(m_impl->nodeMap, "OffsetY"); }
int CameraController::getWidth()       const { return (int)getInt(m_impl->nodeMap, "Width");   }
int CameraController::getHeight()      const { return (int)getInt(m_impl->nodeMap, "Height");  }
int CameraController::getSensorWidth()  const { return 2048; }
int CameraController::getSensorHeight() const { return 2048; }


// ---------------------------------------------------------------------------
// Decimation
// ---------------------------------------------------------------------------

void CameraController::setDecimation(int factor)
{
    setDecimationH(factor);
    setDecimationV(factor);
}

void CameraController::setDecimationH(int factor)
{
    try {
        setInt(m_impl->nodeMap, "DecimationHorizontal", static_cast<int64_t>(factor));
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

void CameraController::setDecimationV(int factor)
{
    try {
        setInt(m_impl->nodeMap, "DecimationVertical", static_cast<int64_t>(factor));
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

int CameraController::getDecimationH() const
{
    return (int)getInt(m_impl->nodeMap, "DecimationHorizontal");
}

int CameraController::getDecimationV() const
{
    return (int)getInt(m_impl->nodeMap, "DecimationVertical");
}


// ---------------------------------------------------------------------------
// Trigger
// ---------------------------------------------------------------------------

void CameraController::setTriggerSource(const std::string& line)
{
    try {
        // Must set TriggerSelector first (this camera uses ExposureStart).
        auto sel = m_impl->nodeMap->FindNode<peak::core::nodes::EnumerationNode>(
            "TriggerSelector");
        sel->SetCurrentEntry("ExposureStart");

        auto src = m_impl->nodeMap->FindNode<peak::core::nodes::EnumerationNode>(
            "TriggerSource");
        src->SetCurrentEntry(line);

        // Enabling the source implicitly means TriggerMode should be On.
        auto mode = m_impl->nodeMap->FindNode<peak::core::nodes::EnumerationNode>(
            "TriggerMode");
        mode->SetCurrentEntry("On");
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

std::string CameraController::getTriggerSource() const
{
    return getEnum(m_impl->nodeMap, "TriggerSource");
}

std::vector<std::string> CameraController::availableTriggerSources() const
{
    // Must select TriggerSelector first or the entries may not be populated.
    try {
        setEnum(m_impl->nodeMap, "TriggerSelector", "ExposureStart");
    } catch (...) {}
    return enumEntries(m_impl->nodeMap, "TriggerSource");
}

void CameraController::setTriggerDivider(int n)
{
    try {
        setEnum(m_impl->nodeMap, "TriggerSelector", "ExposureStart");
        setInt(m_impl->nodeMap, "TriggerDivider", static_cast<int64_t>(n));
    } catch (...) {
        m_impl->lastError = "TriggerDivider not available on this firmware.";
    }
}

int CameraController::getTriggerDivider() const
{
    try {
        return (int)getInt(m_impl->nodeMap, "TriggerDivider");
    } catch (...) {
        return 1;  // safe default
    }
}

void CameraController::setTriggerDelay(double microseconds)
{
    try {
        setEnum(m_impl->nodeMap, "TriggerSelector", "ExposureStart");
        setFloat(m_impl->nodeMap, "TriggerDelay", microseconds);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

double CameraController::getTriggerDelay() const
{
    try{
        return getFloat(m_impl->nodeMap, "TriggerDelay");
    } catch (...) {
        return 0.0;
    }
}

void CameraController::setTriggerEnabled(bool enabled)
{
    try {
        auto trigMode = m_impl->nodeMap->FindNode<peak::core::nodes::EnumerationNode>(
            "TriggerMode");
        trigMode->SetCurrentEntry(enabled ? "On" : "Off");
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

// ---------------------------------------------------------------------------
// Flash / strobe
// ---------------------------------------------------------------------------

void CameraController::setFlashReference(const std::string& ref)
{
    try {
        setEnum(m_impl->nodeMap, "FlashReference", ref);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

void CameraController::setFlashStartDelay(double microseconds)
{
    try {
        setFloat(m_impl->nodeMap, "FlashStartDelay", microseconds);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

void CameraController::setFlashDuration(double microseconds)
{
    try {
        // FlashDuration is only writable when FlashReference = "ExposureStart".
        // If it throws, the caller likely has the wrong FlashReference set.
        setFloat(m_impl->nodeMap, "FlashDuration", microseconds);
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

void CameraController::configureFlashOutput(const std::string& flashReference,
                                            double startDelayUs,
                                            double durationUs)
{
    try {
        // Route "FlashActive" signal to the optocoupled output (Line1).
        // Pins 2 (-) and 5 (+) of the Hirose connector.
        setEnum(m_impl->nodeMap, "LineSelector", "Line1");
        setEnum(m_impl->nodeMap, "LineSource",   "FlashActive");

        setEnum(m_impl->nodeMap,  "FlashReference",  flashReference);
        setFloat(m_impl->nodeMap, "FlashStartDelay", startDelayUs);

        // FlashDuration is only meaningful when FlashReference = "ExposureStart".
        // Skip it silently for "ExposureActive" to avoid a node-access error.
        if (flashReference == "ExposureStart") {
            setFloat(m_impl->nodeMap, "FlashDuration", durationUs);
        }

    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

double CameraController::getFlashStartDelay() const
{
    try{
        return getFloat(m_impl->nodeMap, "FlashStartDelay");
    } catch (...) {
        return 0.0;
    }
}

double CameraController::getFlashDuration() const
{
    try {
        return getFloat(m_impl->nodeMap, "FlashDuration");
    } catch(...) {
        return 0.0;
    }
}


// ---------------------------------------------------------------------------
// Acquisition
// ---------------------------------------------------------------------------

void CameraController::startAcquisition()
{
    if (m_impl->acquiring)
        return;
    try {
        m_impl->dataStream->StartAcquisition();
        execCommand(m_impl->nodeMap, "AcquisitionStart");
        m_impl->acquiring = true;
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

void CameraController::stopAcquisition()
{
    if (!m_impl->acquiring)
        return;
    try {
        execCommand(m_impl->nodeMap, "AcquisitionStop");
        m_impl->dataStream->StopAcquisition(
            peak::core::AcquisitionStopMode::Default);
        m_impl->acquiring = false;
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
    }
}

bool CameraController::isAcquiring() const
{
    return m_impl->acquiring;
}

QImage CameraController::acquireFrame(int timeout_ms)
{
    if (!m_impl->acquiring)
        return QImage{};

    try {
        // Block until a buffer is filled or timeout expires.
        auto buffer = m_impl->dataStream->WaitForFinishedBuffer(
            std::chrono::milliseconds(timeout_ms));

        // Wrap in an IDS image object for format conversion.
        auto rawImage = peak::ipl::Image(
            peak::ipl::PixelFormatName::Mono8,   // adjust if using Mono12
            static_cast<uint8_t*>(buffer->BasePtr()),
            buffer->Size(),
            buffer->Width(),
            buffer->Height());

        // Convert to Mono8 if not already (handles Mono10/12 packed formats).
        auto convertedImage = rawImage.ConvertTo(peak::ipl::PixelFormatName::Mono8,
                                                 peak::ipl::ConversionMode::Fast);

        // Build a QImage that shares the converted image's memory.
        // Copy it immediately so it outlives the buffer.
        QImage qimg(static_cast<const uchar*>(convertedImage.Data()),
                    (int)convertedImage.Width(),
                    (int)convertedImage.Height(),
                    (int)convertedImage.Width(),  // bytes per line for Mono8
                    QImage::Format_Grayscale8);

        QImage result = qimg.copy(); // deep copy before re-queuing the buffer

        // Return buffer to the pool.
        m_impl->dataStream->QueueBuffer(buffer);

        return result;

    } catch (const peak::core::TimeoutException&) {
        // Not an error — just no frame within the timeout window.
        return QImage{};
    } catch (const std::exception& e) {
        m_impl->lastError = e.what();
        return QImage{};
    }
}
