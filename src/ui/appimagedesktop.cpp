#include "ui/appimagedesktop.h"

#include <gio/gio.h>
#include <glib.h>

namespace loadout {
namespace {

bool fail(std::string &error, const std::string &message)
{
    error = message;
    return false;
}

std::string takeError(GError *&error)
{
    std::string message = error ? error->message : "unknown error";
    g_clear_error(&error);
    return message;
}

} // namespace

bool writeAppImageDesktopEntry(const DesktopEntry &entry, std::string &error)
{
    if (entry.id.empty() || entry.executable.empty() || entry.home.empty())
        return fail(error, "the desktop entry is missing its id, executable, or home directory");
    if (entry.iconResource.empty())
        return fail(error, "the desktop entry has no bundled icon");

    const std::string dataDir = entry.home + "/.local/share";
    const std::string applicationsDir = dataDir + "/applications";
    const std::string iconsDir = dataDir + "/icons/hicolor/256x256/apps";

    if (g_mkdir_with_parents(applicationsDir.c_str(), 0755) != 0)
        return fail(error, "could not create " + applicationsDir);
    if (g_mkdir_with_parents(iconsDir.c_str(), 0755) != 0)
        return fail(error, "could not create " + iconsDir);

    GError *gerror = nullptr;
    GBytes *bytes = g_resources_lookup_data(entry.iconResource.c_str(),
                                            G_RESOURCE_LOOKUP_FLAGS_NONE, &gerror);
    if (!bytes)
        return fail(error, "could not read " + entry.iconResource + ": " + takeError(gerror));

    gsize size = 0;
    const void *data = g_bytes_get_data(bytes, &size);
    const std::string iconPath = iconsDir + "/" + entry.id + ".png";
    const bool iconWritten =
        g_file_set_contents(iconPath.c_str(), static_cast<const char *>(data), size, &gerror) != 0;
    const std::string iconError = iconWritten ? std::string{} : takeError(gerror);
    g_bytes_unref(bytes);
    if (!iconWritten)
        return fail(error, "could not write " + iconPath + ": " + iconError);

    const std::string desktopPath = applicationsDir + "/" + entry.id + ".desktop";
    const std::string contents = desktopFileContents(entry);
    if (!g_file_set_contents(desktopPath.c_str(), contents.c_str(), contents.size(), &gerror))
        return fail(error, "could not write " + desktopPath + ": " + takeError(gerror));

    return true;
}

} // namespace loadout
