#pragma once
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

struct SDL_Window;

namespace platform {

// One filter entry: display name + semicolon-separated glob patterns
// without dots (e.g. {"LIB archives", "LIB;lib"}, {"All files", "*"}).
// Patterns carry both cases explicitly — portal glob matching on Linux is
// case-sensitive and FA files are conventionally uppercase.
struct FileFilter {
    const char* name;
    const char* pattern;
};

// Async native file dialogs on top of SDL3. Completion callbacks may fire on
// any thread; results are queued and continuations run on the main thread
// from PumpDialogResults(). One dialog at a time — requests made while one
// is pending are ignored (matches the old modal behaviour).
//
// Cancellation: OpenFilesDialog delivers an empty vector; SaveFileDialog and
// ChooseFolderDialog deliver an empty string.
void DialogsInit(SDL_Window* parent);
void DialogsShutdown();   // drops any pending continuation; call before App teardown
bool DialogBusy();
void PumpDialogResults(); // once per frame on the main thread

void OpenFilesDialog(std::initializer_list<FileFilter> filters, bool allowMany,
                     std::function<void(std::vector<std::string>)> done);

// defaultExt (no dot) is appended when the chosen name has none — replaces
// Win32's lpstrDefExt. defaultLocation may be null.
void SaveFileDialog(std::initializer_list<FileFilter> filters,
                    const char* defaultExt, const char* defaultLocation,
                    std::function<void(std::string)> done);

void ChooseFolderDialog(std::function<void(std::string)> done);

} // namespace platform
