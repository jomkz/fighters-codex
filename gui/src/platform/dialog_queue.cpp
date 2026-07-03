#include "dialog_queue.h"

namespace platform {

bool DialogQueue::Begin(Continuation done) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_busy) return false;
    m_cont = std::move(done);
    m_result.clear();
    m_busy = true;
    m_done = false;
    return true;
}

void DialogQueue::Complete(std::vector<std::string> result) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_busy) return;
    m_result = std::move(result);
    m_done   = true;
}

void DialogQueue::Pump() {
    Continuation cont;
    std::vector<std::string> result;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_busy || !m_done) return;
        cont   = std::move(m_cont);
        result = std::move(m_result);
        m_cont = nullptr;
        m_result.clear();
        m_busy = false;
        m_done = false;
    }
    if (cont) cont(std::move(result));
}

bool DialogQueue::Busy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_busy;
}

void DialogQueue::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cont = nullptr;
    m_result.clear();
    m_busy = false;
    m_done = false;
}

std::string EnsureExtension(std::string path, const char* ext) {
    if (path.empty() || !ext || !*ext) return path;
    auto slash = path.find_last_of("/\\");
    auto dot   = path.rfind('.');
    if (dot == std::string::npos ||
        (slash != std::string::npos && dot < slash)) {
        path += '.';
        path += ext;
    }
    return path;
}

} // namespace platform
