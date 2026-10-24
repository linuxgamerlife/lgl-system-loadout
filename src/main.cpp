#include "ui/themewatcher.h"
#include "ui/window.h"

#include <glib/gstdio.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/application.h>

#include <memory>
#include <string>

namespace {

// Smoke-test hook: with LOADOUT_GTK_SNAPSHOT=<folder>, every visible window is
// rendered to <folder>/<window title>.png once the app has settled, and the
// application quits.
void saveSnapshot(GtkWidget *widget, const std::string &path)
{
    const int width = gtk_widget_get_width(widget);
    const int height = gtk_widget_get_height(widget);
    GdkPaintable *paintable = gtk_widget_paintable_new(widget);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot), width, height);
    if (GskRenderNode *node = gtk_snapshot_free_to_node(snapshot)) {
        GskRenderer *renderer = gtk_native_get_renderer(GTK_NATIVE(widget));
        const graphene_rect_t bounds = GRAPHENE_RECT_INIT(
            0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        GdkTexture *texture = gsk_renderer_render_texture(renderer, node, &bounds);
        gdk_texture_save_to_png(texture, path.c_str());
        g_object_unref(texture);
        gsk_render_node_unref(node);
    }
    g_object_unref(paintable);
}

void scheduleSnapshot(const Glib::RefPtr<Gtk::Application> &application)
{
    const std::string folder = Glib::getenv("LOADOUT_GTK_SNAPSHOT");
    if (folder.empty())
        return;

    Glib::signal_timeout().connect_once(
        [application, folder] {
            g_mkdir_with_parents(folder.c_str(), 0755);

            GListModel *toplevels = gtk_window_get_toplevels();
            for (guint i = 0; i < g_list_model_get_n_items(toplevels); ++i) {
                auto *window = static_cast<GtkWidget *>(g_list_model_get_item(toplevels, i));
                const char *title = gtk_window_get_title(GTK_WINDOW(window));
                std::string name = title && *title ? title : "window-" + std::to_string(i);
                for (char &c : name) {
                    if (!g_ascii_isalnum(c))
                        c = '-';
                }
                if (gtk_widget_get_visible(window) && gtk_widget_get_realized(window))
                    saveSnapshot(window, Glib::build_filename(folder, name + ".png"));
                g_object_unref(window);
            }
            application->quit();
        },
        3000);
}

} // namespace

int main(int argc, char *argv[])
{
    const std::string snapshotFolder = Glib::getenv("LOADOUT_GTK_SNAPSHOT");
    auto application =
        snapshotFolder.empty()
            ? Gtk::Application::create("com.linuxgamerlife.lgl-system-loadout")
            // Snapshot runs must not hand off to an already running instance.
            : Gtk::Application::create("com.linuxgamerlife.lgl-system-loadout",
                                       Gio::Application::Flags::NON_UNIQUE);

    std::unique_ptr<loadout::Window> window;

    application->signal_startup().connect([&] {
        Glib::set_application_name("LGL System Loadout");
        Gtk::Window::set_default_icon_name("lgl-system-loadout");
        loadout::installApplicationStylesheet();
        application->add_action("quit", [&] {
            if (window)
                window->set_visible(false);
            application->quit();
        });
        application->set_accel_for_action("app.quit", "<Control>q");
    });

    application->signal_activate().connect([&] {
        if (!window)
            window = std::make_unique<loadout::Window>(application);
        window->present();
        scheduleSnapshot(application);
    });

    const int status = application->run(argc, argv);
    window.reset();
    return status;
}
