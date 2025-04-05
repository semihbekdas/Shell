#include <gtk/gtk.h>
#include <string.h>


typedef struct {
    GtkTextBuffer *buffer;
    guint counter;
} AppData;

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (keyval != GDK_KEY_Return)
        return GDK_EVENT_PROPAGATE;

    AppData *app_data = user_data;
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(app_data->buffer, &end_iter);

    GtkTextIter line_start = end_iter;
    gtk_text_iter_set_line_offset(&line_start, 0);
    gchar *input = gtk_text_buffer_get_text(app_data->buffer, &line_start, &end_iter, FALSE);

    if (g_strcmp0(input, "") == 0) {
        g_free(input);
        return GDK_EVENT_STOP;
    }

    gchar *output1 = g_strdup_printf("%s\n" ,input);
    gchar *output2 = g_strdup_printf("%u: %s\n", app_data->counter++, input);

    gtk_text_buffer_delete(app_data->buffer, &line_start, &end_iter);
    gtk_text_buffer_insert(app_data->buffer, &line_start, output1, -1);
    gtk_text_buffer_insert(app_data->buffer, &line_start, output2, -1);

    g_free(input);
    g_free(output1);
    g_free(output2);

    return GDK_EVENT_STOP;
}

// Her sekme için terminal işlevselliğini oluşturan fonksiyon
GtkWidget *create_terminal_tab() {
    // Terminal sekmesi için kapsayıcı (scrolled window içinde text view)
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);

    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);

    // Her sekme için ayrı AppData oluşturuluyor
    AppData *app_data = g_new0(AppData, 1);
    app_data->counter = 1;
    app_data->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    // Klavye kontrolcüsü ekliyoruz
    GtkEventController *key_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(text_view, key_controller);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), app_data);


    return scrolled_window;
}

static void on_add_tab_clicked(GtkButton *button, gpointer user_data) {
    GtkNotebook *notebook = GTK_NOTEBOOK(user_data);
    // Yeni terminal sekmesini oluşturuyoruz
    GtkWidget *terminal_tab = create_terminal_tab();
    // Sekmeye başlık olarak "Terminal" ifadesini veriyoruz.
    // (Not: Başlığı dinamik hale getirebilir, örneğin sekme sayısına göre adlandırabilirsiniz)
    static int counter1 = 2;
    char head[20];
    sprintf(head, "Terminal%d", counter1);
    gchar *label_text = g_strdup(head);
    counter1++;
    GtkWidget *label = gtk_label_new(label_text);
    g_free(label_text);

    // Yeni sekmeyi notebook'a ekliyoruz
    gtk_notebook_append_page(notebook, terminal_tab, label);
    gtk_widget_set_visible(terminal_tab, TRUE);
    gtk_widget_set_visible(label, TRUE);
    // Eklenen sekmeye otomatik geçiş yapıyoruz
    gtk_notebook_set_current_page(notebook, gtk_notebook_get_n_pages(notebook) - 1);
}

static void activate(GtkApplication *app, gpointer user_data) {
    // Ana pencere
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Terminal");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Ana dikey düzen kutusu (üstte header, altta notebook)
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // Üst kısım: "+" butonu ile yeni sekme ekleme
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(vbox), hbox);
    GtkWidget *add_tab_button = gtk_button_new_with_label("+");
    gtk_box_append(GTK_BOX(hbox), add_tab_button);

    // Sekmeleri içerecek Notebook
    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_append(GTK_BOX(vbox), notebook);

    // İlk sekmeyi ekleyelim
    GtkWidget *first_tab = create_terminal_tab();
    GtkWidget *first_label = gtk_label_new("Terminal1");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), first_tab, first_label);

    // "+" butonuna tıklandığında yeni sekme ekle
    g_signal_connect(add_tab_button, "clicked", G_CALLBACK(on_add_tab_clicked), notebook);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.GtkTabbedTerminal", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
