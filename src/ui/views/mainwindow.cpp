#include "mainwindow.hpp"
#include <chrono>
#include <thread>
#include <utility>
#include "adddownloaddialog.hpp"
#include "preferencesdialog.hpp"
#include "shortcutsdialog.hpp"
#include "../controls/messagedialog.hpp"
#include "../controls/progressdialog.hpp"
#include "../../helpers/stringhelpers.hpp"
#include "../../helpers/translation.hpp"

using namespace NickvisionTubeConverter::Controllers;
using namespace NickvisionTubeConverter::Helpers;
using namespace NickvisionTubeConverter::Models;
using namespace NickvisionTubeConverter::UI::Controls;
using namespace NickvisionTubeConverter::UI::Views;

MainWindow::MainWindow(GtkApplication* application, const MainWindowController& controller) : m_controller{ controller }, m_gobj{ adw_application_window_new(application) }
{
    //Window Settings
    gtk_window_set_default_size(GTK_WINDOW(m_gobj), 900, 700);
    if(m_controller.getIsDevVersion())
    {
        gtk_style_context_add_class(gtk_widget_get_style_context(m_gobj), "devel");
    }
    g_signal_connect(m_gobj, "close_request", G_CALLBACK((bool (*)(GtkWindow*, gpointer))[](GtkWindow*, gpointer data) -> bool { return reinterpret_cast<MainWindow*>(data)->onCloseRequest(); }), this);
    //Header Bar
    m_headerBar = adw_header_bar_new();
    m_adwTitle = adw_window_title_new(m_controller.getAppInfo().getShortName().c_str(), nullptr);
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(m_headerBar), m_adwTitle);
    //Add Download Button
    m_btnAddDownload = gtk_button_new();
    GtkWidget* btnAddDownloadContent{ adw_button_content_new() };
    adw_button_content_set_icon_name(ADW_BUTTON_CONTENT(btnAddDownloadContent), "list-add-symbolic");
    adw_button_content_set_label(ADW_BUTTON_CONTENT(btnAddDownloadContent), _("Add"));
    gtk_button_set_child(GTK_BUTTON(m_btnAddDownload), btnAddDownloadContent);
    gtk_widget_set_tooltip_text(m_btnAddDownload, _("Add Download (Ctrl+N)"));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(m_btnAddDownload), "win.addDownload");
    adw_header_bar_pack_start(ADW_HEADER_BAR(m_headerBar), m_btnAddDownload);
    //Menu Help Button
    m_btnMenuHelp = gtk_menu_button_new();
    GMenu* menuHelp{ g_menu_new() };
    g_menu_append(menuHelp, _("Preferences"), "win.preferences");
    g_menu_append(menuHelp, _("Keyboard Shortcuts"), "win.keyboardShortcuts");
    g_menu_append(menuHelp, std::string(StringHelpers::format(_("About %s"), m_controller.getAppInfo().getShortName().c_str())).c_str(), "win.about");
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(m_btnMenuHelp), GTK_ARROW_NONE);
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(m_btnMenuHelp), G_MENU_MODEL(menuHelp));
    gtk_widget_set_tooltip_text(m_btnMenuHelp, _("Main Menu"));
    adw_header_bar_pack_end(ADW_HEADER_BAR(m_headerBar), m_btnMenuHelp);
    g_object_unref(menuHelp);
    //Toast Overlay
    m_toastOverlay = adw_toast_overlay_new();
    gtk_widget_set_hexpand(m_toastOverlay, true);
    gtk_widget_set_vexpand(m_toastOverlay, true);
    //Page No Downloads
    m_pageStatusNoDownloads = adw_status_page_new();
    adw_status_page_set_icon_name(ADW_STATUS_PAGE(m_pageStatusNoDownloads), "org.nickvision.tubeconverter-symbolic");
    adw_status_page_set_title(ADW_STATUS_PAGE(m_pageStatusNoDownloads), _("No Downloads"));
    adw_status_page_set_description(ADW_STATUS_PAGE(m_pageStatusNoDownloads), _("Add a download to get started."));
    //Page Downloads
    m_grpDownloads = adw_preferences_group_new();
    gtk_widget_set_margin_start(m_grpDownloads, 30);
    gtk_widget_set_margin_top(m_grpDownloads, 10);
    gtk_widget_set_margin_end(m_grpDownloads, 30);
    gtk_widget_set_margin_bottom(m_grpDownloads, 10);
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(m_grpDownloads), _("Downloads"));
    m_pageScrollDownloads = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(m_pageScrollDownloads, true);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_pageScrollDownloads), m_grpDownloads);
    //View Stack
    m_viewStack = adw_view_stack_new();
    adw_view_stack_add_named(ADW_VIEW_STACK(m_viewStack), m_pageStatusNoDownloads, "pageNoDownloads");
    adw_view_stack_add_named(ADW_VIEW_STACK(m_viewStack), m_pageScrollDownloads, "pageDownloads");
    adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(m_toastOverlay), m_viewStack);
    //Main Box
    m_mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(m_mainBox), m_headerBar);
    gtk_box_append(GTK_BOX(m_mainBox), m_toastOverlay);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(m_gobj), m_mainBox);
    //Send Toast Callback
    m_controller.registerSendToastCallback([&](const std::string& message) { adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(m_toastOverlay), adw_toast_new(message.c_str())); });
    //Add Download Action
    m_actAddDownload = g_simple_action_new("addDownload", nullptr);
    g_signal_connect(m_actAddDownload, "activate", G_CALLBACK((void (*)(GSimpleAction*, GVariant*, gpointer))[](GSimpleAction*, GVariant*, gpointer data) { reinterpret_cast<MainWindow*>(data)->onAddDownload(); }), this);
    g_action_map_add_action(G_ACTION_MAP(m_gobj), G_ACTION(m_actAddDownload));
    gtk_application_set_accels_for_action(application, "win.addDownload", new const char*[2]{ "<Ctrl>n", nullptr });
    //Preferences Action
    m_actPreferences = g_simple_action_new("preferences", nullptr);
    g_signal_connect(m_actPreferences, "activate", G_CALLBACK((void (*)(GSimpleAction*, GVariant*, gpointer))[](GSimpleAction*, GVariant*, gpointer data) { reinterpret_cast<MainWindow*>(data)->onPreferences(); }), this);
    g_action_map_add_action(G_ACTION_MAP(m_gobj), G_ACTION(m_actPreferences));
    gtk_application_set_accels_for_action(application, "win.preferences", new const char*[2]{ "<Ctrl>comma", nullptr });
    //Keyboard Shortcuts Action
    m_actKeyboardShortcuts = g_simple_action_new("keyboardShortcuts", nullptr);
    g_signal_connect(m_actKeyboardShortcuts, "activate", G_CALLBACK((void (*)(GSimpleAction*, GVariant*, gpointer))[](GSimpleAction*, GVariant*, gpointer data) { reinterpret_cast<MainWindow*>(data)->onKeyboardShortcuts(); }), this);
    g_action_map_add_action(G_ACTION_MAP(m_gobj), G_ACTION(m_actKeyboardShortcuts));
    gtk_application_set_accels_for_action(application, "win.keyboardShortcuts", new const char*[2]{ "<Ctrl>question", nullptr });
    //About Action
    m_actAbout = g_simple_action_new("about", nullptr);
    g_signal_connect(m_actAbout, "activate", G_CALLBACK((void (*)(GSimpleAction*, GVariant*, gpointer))[](GSimpleAction*, GVariant*, gpointer data) { reinterpret_cast<MainWindow*>(data)->onAbout(); }), this);
    g_action_map_add_action(G_ACTION_MAP(m_gobj), G_ACTION(m_actAbout));
    gtk_application_set_accels_for_action(application, "win.about", new const char*[2]{ "F1", nullptr });
}

GtkWidget* MainWindow::gobj()
{
    return m_gobj;
}

void MainWindow::start()
{
    gtk_widget_show(m_gobj);
    m_controller.startup();
}

bool MainWindow::onCloseRequest()
{
    if(m_controller.getIsDownloadsRunning())
    {
        MessageDialog messageDialog{ GTK_WINDOW(m_gobj), _("Close and Stop Downloads?"), _("Some downloads are still in progress. Are you sure you want to close Tube Converter and stop the running downloads?"), _("No"), _("Yes") };
        if(messageDialog.run() == MessageDialogResponse::Cancel)
        {
            return true;
        }
    }
    m_controller.stopDownloads();
    m_downloadRows.clear();
    return false;
}

void MainWindow::onAddDownload()
{
    AddDownloadDialogController addDownloadDialogController{ m_controller.createAddDownloadDialogController() };
    AddDownloadDialog addDownloadDialog{ GTK_WINDOW(m_gobj), addDownloadDialogController };
    if(addDownloadDialog.run())
    {
        adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(m_viewStack), "pageDownloads");
        const std::shared_ptr<Download>& download{ addDownloadDialogController.getDownload() };
        std::unique_ptr<DownloadRow> row{ std::make_unique<DownloadRow>(GTK_WINDOW(m_gobj), download) };
        m_controller.addDownload(download);
        adw_preferences_group_add(ADW_PREFERENCES_GROUP(m_grpDownloads), row->gobj());
        row->start(m_controller.getEmbedMetadata());
        m_downloadRows.push_back(std::move(row));
    }
}

void MainWindow::onPreferences()
{
    PreferencesDialog preferencesDialog{ GTK_WINDOW(m_gobj), m_controller.createPreferencesDialogController() };
    preferencesDialog.run();
}

void MainWindow::onKeyboardShortcuts()
{
    ShortcutsDialog shortcutsDialog{ GTK_WINDOW(m_gobj) };
    shortcutsDialog.run();
}

void MainWindow::onAbout()
{
    adw_show_about_window(GTK_WINDOW(m_gobj),
                          "application-name", m_controller.getAppInfo().getShortName().c_str(),
                          "application-icon", (m_controller.getAppInfo().getId() + (m_controller.getIsDevVersion() ? "-devel" : "")).c_str(),
                          "version", m_controller.getAppInfo().getVersion().c_str(),
                          "comments", m_controller.getAppInfo().getDescription().c_str(),
                          "developer-name", "Nickvision",
                          "license-type", GTK_LICENSE_GPL_3_0,
                          "copyright", std::string(std::string("(C) Nickvision 2021-2022\n\n") + _("DISCLAIMER: The authors of Tube Converter are not responsible/liable for any misuse of this program that may violate local copyright/DMCA laws. Users use this application at their own risk.")).c_str(),
                          "website", m_controller.getAppInfo().getGitHubRepo().c_str(),
                          "issue-url", m_controller.getAppInfo().getIssueTracker().c_str(),
                          "support-url", m_controller.getAppInfo().getSupportUrl().c_str(),
                          "developers", new const char*[3]{ "Nicholas Logozzo https://github.com/nlogozzo", "Contributors on GitHub ❤️ https://github.com/nlogozzo/NickvisionTubeConverter/graphs/contributors", nullptr },
                          "designers", new const char*[2]{ "Nicholas Logozzo https://github.com/nlogozzo", nullptr },
                          "artists", new const char*[3]{ "David Lapshin https://github.com/daudix-UFO", "marcin https://github.com/martin-desktops", nullptr },
                          "debug-info", "Dependencies:\n- yt-dlp Version 2022.10.04\n- ffmpeg Version 5.1.2",
                          "release-notes", m_controller.getAppInfo().getChangelog().c_str(),
                          nullptr);
}



