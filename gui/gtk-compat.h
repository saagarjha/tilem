#if !GTK_CHECK_VERSION(2, 14, 0)
# define gtk_dialog_get_content_area(d) ((d)->vbox)
# define gtk_widget_get_window(w) ((w)->window)
#endif
#if !GTK_CHECK_VERSION(2, 18, 0)
# define gtk_widget_get_allocation(w, a) (*(a) = (w)->allocation)
# define gtk_widget_get_visible(w) GTK_WIDGET_VISIBLE(w)
# define gtk_widget_set_can_default(w, v) g_object_set((w), "can-default", v, NULL)
#endif
