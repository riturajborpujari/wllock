#include "glib.h"
#include <gtk/gtk.h>
#include <gtk/gtkvideo.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <gtk4-layer-shell/gtk4-session-lock.h>
#include <security/_pam_types.h>
#include <security/pam_appl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    GtkApplication         *app;
    GtkWindow              *lock_window;
    GtkSessionLockInstance *lock;
} AppData;
GtkWidget *password_entry;
GtkWidget *message_label;
GtkWidget *clock_label;

static gboolean update_clock(gpointer user_data) {
    GtkLabel  *label    = GTK_LABEL(user_data);
    time_t     rawtime  = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);

    char buffer[64];
    strftime(buffer, sizeof(buffer), "Its <b>%b %d, %H:%M</b>", timeinfo);
    gtk_label_set_markup(label, buffer);

    return TRUE;
}

static int pam_converstaion(int num_msg, const struct pam_message **msg,
                            struct pam_response **resp, void *appdata_ptr) {
    const char          *password = (const char *)appdata_ptr;
    struct pam_response *replies = calloc(num_msg, sizeof(struct pam_response));

    if (replies == NULL) {
        return PAM_CONV_ERR;
    }

    for (int i = 0; i < num_msg; i++) {
        if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            replies[i].resp = strdup(password);
        } else {
            replies[i].resp = strdup("");
        }
    }

    *resp = replies;
    return PAM_SUCCESS;
}

static gboolean verify_password(const char *password) {
    struct pam_conv conv = {
        pam_converstaion,
        (void *)password,
    };
    pam_handle_t *pamh = NULL;

    int status = pam_start("login", getlogin(), &conv, &pamh);
    if (status != PAM_SUCCESS) {
        return FALSE;
    }

    status = pam_authenticate(pamh, 0);
    pam_end(pamh, status);

    return status == PAM_SUCCESS;
}

static void on_password_entered(GtkWidget *widget, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    // FIXME: the following label text set doesn't work
    gtk_label_set_text(GTK_LABEL(message_label), "Verifying...");

    const char *entered_password =
        gtk_editable_get_text(GTK_EDITABLE(password_entry));
    gtk_editable_set_editable(GTK_EDITABLE(password_entry), FALSE);

    if (data->lock) {
        // TODO: Move PAM verification to separate thread
        //          ? GTK main loop locks on pam verification.
        if (verify_password(entered_password)) {
            gtk_session_lock_instance_unlock(data->lock);
            gtk_window_destroy(data->lock_window);
        } else {
            gtk_editable_set_text(GTK_EDITABLE(password_entry), "");
            gtk_label_set_text(GTK_LABEL(message_label), "Wrong Password!");
            gtk_editable_set_editable(GTK_EDITABLE(password_entry), TRUE);
        }
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data->lock_window) {
        data->lock_window = GTK_WINDOW(gtk_application_window_new(app));
    }
    gtk_window_set_title(data->lock_window, "Lockscreen");
    gtk_window_set_default_size(data->lock_window, -1, -1);

    gtk_layer_init_for_window(data->lock_window);
    gtk_layer_set_layer(data->lock_window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_auto_exclusive_zone_enable(data->lock_window);
    gtk_layer_set_anchor(data->lock_window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(data->lock_window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(data->lock_window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(data->lock_window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_keyboard_mode(
        GTK_WINDOW(data->lock_window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_window_set_child(data->lock_window, overlay);

	char *media_filepath = getenv("WLLOCK_MEDIA_FILE");
	if (media_filepath == NULL) {
		fprintf(stderr, "ERROR: no media file path\n");
		fprintf(stdout, "ensure env `WLLOCK_MEDIA_FILE` is set to the absolute path "
				"of an image or video file\n");
		g_application_quit(G_APPLICATION(app));
		return;
	}
    GtkMediaStream *media = gtk_media_file_new_for_filename(media_filepath);
    GtkWidget *bg_image = gtk_picture_new_for_paintable(GDK_PAINTABLE(media));
    gtk_media_stream_play(GTK_MEDIA_STREAM(media));
    gtk_media_stream_set_loop(GTK_MEDIA_STREAM(media), true);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_image);

    GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(container, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(container, 800, -1);
    gtk_widget_set_margin_top(container, 256);

    password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(password_entry), "password please");
    gtk_entry_set_icon_from_icon_name(
        GTK_ENTRY(password_entry), GTK_ENTRY_ICON_PRIMARY, "dialog-password");
    gtk_widget_set_hexpand(password_entry, TRUE);
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(password_entry), '*');
    g_signal_connect(
        password_entry, "activate", G_CALLBACK(on_password_entered), data);

    char *greeting_text = g_strdup_printf(
        "Hi there, <span color=\"orange\">%s</span>", getlogin());
    GtkWidget *greeting_label = gtk_label_new("");
    gtk_widget_set_halign(greeting_label, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(greeting_label), greeting_text);

    clock_label = gtk_label_new("");
    gtk_widget_set_halign(clock_label, GTK_ALIGN_START);
    update_clock(clock_label);
    g_timeout_add_seconds(1, update_clock, clock_label);

    message_label = gtk_label_new("");
    gtk_widget_set_halign(message_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(message_label, "message");

    gtk_box_append(GTK_BOX(container), greeting_label);
    gtk_box_append(GTK_BOX(container), clock_label);
    gtk_box_append(GTK_BOX(container), password_entry);
    gtk_box_append(GTK_BOX(container), message_label);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), container);

    GtkCssProvider *provider = gtk_css_provider_new();
    char            user_styles_path[1024];
    sprintf(user_styles_path, "%s/.config/wllock/style.css", getenv("HOME"));
    gtk_css_provider_load_from_path(provider, user_styles_path);
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(GTK_WIDGET(data->lock_window)),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GdkDisplay *display = gdk_display_get_default();

    GListModel *monitors = gdk_display_get_monitors(display);
    GdkMonitor *monitor  = g_list_model_get_item(monitors, 0);
    if (!data->lock) {
        data->lock = gtk_session_lock_instance_new();
    }
    if (!gtk_session_lock_instance_is_locked(data->lock)) {
        gtk_session_lock_instance_lock(data->lock);
    }
    gtk_session_lock_instance_assign_window_to_monitor(
        data->lock, data->lock_window, monitor);
    gtk_window_present(GTK_WINDOW(data->lock_window));
}

int main(int argc, char *argv[]) {
    AppData data = {0};

    GtkApplication *app = gtk_application_new(
        "org.roodrax.wllock", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &data);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
