#include "menuframe.h"
#include "client.h"
#include "menu.h"
#include "screen.h"
#include "grab.h"
#include "openbox.h"
#include "render/theme.h"

#define SEPARATOR_HEIGHT 5

#define FRAME_EVENTMASK (ButtonPressMask |ButtonMotionMask | EnterWindowMask |\
			 LeaveWindowMask)
#define TITLE_EVENTMASK (ButtonPressMask | ButtonMotionMask)
#define ENTRY_EVENTMASK (EnterWindowMask | LeaveWindowMask | \
                         ButtonPressMask | ButtonReleaseMask)

GList *menu_frame_visible;

static ObMenuEntryFrame* menu_entry_frame_new(ObMenuEntry *entry,
                                              ObMenuFrame *frame);
static void menu_entry_frame_free(ObMenuEntryFrame *self);
static void menu_frame_render(ObMenuFrame *self);
static void menu_frame_update(ObMenuFrame *self);

static Window createWindow(Window parent, unsigned long mask,
			   XSetWindowAttributes *attrib)
{
    return XCreateWindow(ob_display, parent, 0, 0, 1, 1, 0,
			 RrDepth(ob_rr_inst), InputOutput,
                         RrVisual(ob_rr_inst), mask, attrib);
                       
}

ObMenuFrame* menu_frame_new(ObMenu *menu, ObClient *client)
{
    ObMenuFrame *self;
    XSetWindowAttributes attr;

    self = g_new0(ObMenuFrame, 1);
    self->type = Window_Menu;
    self->menu = menu;
    self->selected = NULL;
    self->show_title = TRUE;
    self->client = client;

    attr.event_mask = FRAME_EVENTMASK;
    self->window = createWindow(RootWindow(ob_display, ob_screen),
                                   CWEventMask, &attr);
    attr.event_mask = TITLE_EVENTMASK;
    self->title = createWindow(self->window, CWEventMask, &attr);
    self->items = createWindow(self->window, 0, NULL);

    XMapWindow(ob_display, self->items);

    self->a_title = RrAppearanceCopy(ob_rr_theme->a_menu_title);
    self->a_items = RrAppearanceCopy(ob_rr_theme->a_menu);

    stacking_add(MENU_AS_WINDOW(self));

    return self;
}

void menu_frame_free(ObMenuFrame *self)
{
    if (self) {
        while (self->entries) {
            menu_entry_frame_free(self->entries->data);
            self->entries = g_list_delete_link(self->entries, self->entries);
        }

        stacking_remove(MENU_AS_WINDOW(self));

        XDestroyWindow(ob_display, self->items);
        XDestroyWindow(ob_display, self->title);
        XDestroyWindow(ob_display, self->window);

        RrAppearanceFree(self->a_items);
        RrAppearanceFree(self->a_title);

        g_free(self);
    }
}

static ObMenuEntryFrame* menu_entry_frame_new(ObMenuEntry *entry,
                                              ObMenuFrame *frame)
{
    ObMenuEntryFrame *self;
    XSetWindowAttributes attr;

    self = g_new0(ObMenuEntryFrame, 1);
    self->entry = entry;
    self->frame = frame;

    attr.event_mask = ENTRY_EVENTMASK;
    self->window = createWindow(self->frame->items, CWEventMask, &attr);
    self->icon = createWindow(self->window, 0, NULL);
    self->text = createWindow(self->window, 0, NULL);
    self->bullet = createWindow(self->window, 0, NULL);

    XMapWindow(ob_display, self->window);
    XMapWindow(ob_display, self->text);

    self->a_normal = RrAppearanceCopy(ob_rr_theme->a_menu_item);
    self->a_disabled = RrAppearanceCopy(ob_rr_theme->a_menu_disabled);
    self->a_selected = RrAppearanceCopy(ob_rr_theme->a_menu_hilite);

    self->a_icon = RrAppearanceCopy(ob_rr_theme->a_clear_tex);
    self->a_icon->texture[0].type = RR_TEXTURE_RGBA;
    self->a_bullet = RrAppearanceCopy(ob_rr_theme->a_menu_bullet);
    self->a_bullet->texture[0].type = RR_TEXTURE_MASK;

    self->a_text_normal =
        RrAppearanceCopy(ob_rr_theme->a_menu_text_item);
    self->a_text_disabled =
        RrAppearanceCopy(ob_rr_theme->a_menu_text_disabled);
    self->a_text_selected =
        RrAppearanceCopy(ob_rr_theme->a_menu_text_hilite);

    return self;
}

static void menu_entry_frame_free(ObMenuEntryFrame *self)
{
    if (self) {
        XDestroyWindow(ob_display, self->icon);
        XDestroyWindow(ob_display, self->text);
        XDestroyWindow(ob_display, self->bullet);
        XDestroyWindow(ob_display, self->window);

        RrAppearanceFree(self->a_normal);
        RrAppearanceFree(self->a_disabled);
        RrAppearanceFree(self->a_selected);

        RrAppearanceFree(self->a_icon);
        RrAppearanceFree(self->a_text_normal);
        RrAppearanceFree(self->a_text_disabled);
        RrAppearanceFree(self->a_text_selected);
        RrAppearanceFree(self->a_bullet);

        g_free(self);
    }
}

void menu_frame_move(ObMenuFrame *self, gint x, gint y)
{
    RECT_SET_POINT(self->area, x, y);
    XMoveWindow(ob_display, self->window, self->area.x, self->area.y);
}

void menu_frame_move_on_screen(ObMenuFrame *self)
{
    Rect *a;
    guint i;
    gint dx = 0, dy = 0;

    for (i = 0; i < screen_num_monitors; ++i) {
        a = screen_physical_area_monitor(i);
        if (RECT_INTERSECTS_RECT(*a, self->area))
            break;
    }
    if (a) a = screen_physical_area_monitor(0);

    dx = MIN(0, (a->x + a->width) - (self->area.x + self->area.width));
    dy = MIN(0, (a->y + a->height) - (self->area.y + self->area.height));
    if (!dx) dx = MAX(0, a->x - self->area.x);
    if (!dy) dy = MAX(0, a->y - self->area.y);

    if (dx || dy) {
        ObMenuFrame *f;

        for (f = self; f; f = f->parent)
            menu_frame_move(f, f->area.x + dx, f->area.y + dy);
        for (f = self->child; f; f = f->child)
            menu_frame_move(f, f->area.x + dx, f->area.y + dy);
        XWarpPointer(ob_display, None, None, 0, 0, 0, 0, dx, dy);
    }
}

static void menu_entry_frame_render(ObMenuEntryFrame *self)
{
    RrAppearance *item_a, *text_a;
    gint th; /* temp */
    ObMenu *sub;

    item_a = ((self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
               !self->entry->data.normal.enabled) ?
              self->a_disabled :
              (self == self->frame->selected ?
               self->a_selected :
               self->a_normal));
    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        th = self->frame->item_h;
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        th = SEPARATOR_HEIGHT;
        break;
    }
    RECT_SET_SIZE(self->area, self->frame->inner_w, th);
    XResizeWindow(ob_display, self->window,
                  self->area.width, self->area.height);
    item_a->surface.parent = self->frame->a_items;
    item_a->surface.parentx = self->area.x;
    item_a->surface.parenty = self->area.y;
    RrPaint(item_a, self->window, self->area.width, self->area.height);

    text_a = ((self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
               !self->entry->data.normal.enabled) ?
              self->a_text_disabled :
              (self == self->frame->selected ?
               self->a_text_selected :
               self->a_text_normal));
    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
        text_a->texture[0].data.text.string = self->entry->data.normal.label;
        break;
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        sub = self->entry->data.submenu.submenu;
        text_a->texture[0].data.text.string = sub ? sub->title : "";
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        break;
    }

    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
        XMoveResizeWindow(ob_display, self->text,
                          self->frame->text_x, 0,
                          self->frame->text_w, self->frame->item_h);
        text_a->surface.parent = item_a;
        text_a->surface.parentx = self->frame->text_x;
        text_a->surface.parenty = 0;
        RrPaint(text_a, self->text, self->frame->text_w, self->frame->item_h);
        XMapWindow(ob_display, self->text);
        break;
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        XMoveResizeWindow(ob_display, self->text,
                          self->frame->text_x, 0,
                          self->frame->text_w - self->frame->item_h,
                          self->frame->item_h);
        text_a->surface.parent = item_a;
        text_a->surface.parentx = self->frame->text_x;
        text_a->surface.parenty = 0;
        RrPaint(text_a, self->text, self->frame->text_w - self->frame->item_h,
                self->frame->item_h);
        XMapWindow(ob_display, self->text);
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        XUnmapWindow(ob_display, self->text);
        break;
    }

    if (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
        self->entry->data.normal.icon_data)
    {
        XMoveResizeWindow(ob_display, self->icon, 0, 0,
                          self->frame->item_h, self->frame->item_h);
        self->a_icon->texture[0].data.rgba.width =
            self->entry->data.normal.icon_width;
        self->a_icon->texture[0].data.rgba.height =
            self->entry->data.normal.icon_height;
        self->a_icon->texture[0].data.rgba.data =
            self->entry->data.normal.icon_data;
        self->a_icon->surface.parent = item_a;
        self->a_icon->surface.parentx = 0;
        self->a_icon->surface.parenty = 0;
        RrPaint(self->a_icon, self->icon,
                self->frame->item_h, self->frame->item_h);
        XMapWindow(ob_display, self->icon);
    } else
        XUnmapWindow(ob_display, self->icon);

    if (self->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
        XMoveResizeWindow(ob_display, self->bullet,
                          self->frame->text_x + self->frame->text_w
                          - self->frame->item_h, 0,
                          self->frame->item_h, self->frame->item_h);
        self->a_bullet->surface.parent = item_a;
        self->a_bullet->surface.parentx =
            self->frame->text_x + self->frame->text_w - self->frame->item_h;
        self->a_bullet->surface.parenty = 0;
        RrPaint(self->a_bullet, self->bullet,
                self->frame->item_h, self->frame->item_h);
        XMapWindow(ob_display, self->bullet);
    } else
        XUnmapWindow(ob_display, self->bullet);
}

static void menu_frame_render(ObMenuFrame *self)
{
    gint w = 0, h = 0;
    gint allitems_h = 0;
    gint tw, th; /* temps */
    GList *it;
    gboolean has_icon = FALSE;
    ObMenu *sub;

    XSetWindowBorderWidth(ob_display, self->window, ob_rr_theme->bwidth);
    XSetWindowBorder(ob_display, self->window,
                     RrColorPixel(ob_rr_theme->b_color));

    if (!self->parent && self->show_title) {
        XMoveWindow(ob_display, self->title, 
                    -ob_rr_theme->bwidth, h - ob_rr_theme->bwidth);

        self->a_title->texture[0].data.text.string = self->menu->title;
        RrMinsize(self->a_title, &tw, &th);
        w = MAX(w, tw);
        h += (self->title_h = th + ob_rr_theme->bwidth);

        XSetWindowBorderWidth(ob_display, self->title, ob_rr_theme->bwidth);
        XSetWindowBorder(ob_display, self->title,
                         RrColorPixel(ob_rr_theme->b_color));
    }

    XMoveWindow(ob_display, self->items, 0, h);

    if (self->entries) {
        ObMenuEntryFrame *e = self->entries->data;
        e->a_text_normal->texture[0].data.text.string = "";
        RrMinsize(e->a_text_normal, &tw, &th);
        self->item_h = th;
    } else
        self->item_h = 0;

    for (it = self->entries; it; it = g_list_next(it)) {
        RrAppearance *text_a;
        ObMenuEntryFrame *e = it->data;

        RECT_SET_POINT(e->area, 0, allitems_h);
        XMoveWindow(ob_display, e->window, 0, e->area.y);

        text_a = ((e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
                   !e->entry->data.normal.enabled) ?
                  e->a_text_disabled :
                  (e == self->selected ?
                   e->a_text_selected :
                   e->a_text_normal));
        switch (e->entry->type) {
        case OB_MENU_ENTRY_TYPE_NORMAL:
            text_a->texture[0].data.text.string = e->entry->data.normal.label;
            RrMinsize(text_a, &tw, &th);

            if (e->entry->data.normal.icon_data)
                has_icon = TRUE;
            break;
        case OB_MENU_ENTRY_TYPE_SUBMENU:
            sub = e->entry->data.submenu.submenu;
            text_a->texture[0].data.text.string = sub ? sub->title : "";
            RrMinsize(text_a, &tw, &th);

            tw += self->item_h;
            break;
        case OB_MENU_ENTRY_TYPE_SEPARATOR:
            tw = 0;
            th = SEPARATOR_HEIGHT;
            break;
        }
        w = MAX(w, tw);
        h += th;
        allitems_h += th;
    }

    self->text_x = 0;
    self->text_w = w;

    if (self->entries) {
        if (has_icon) {
            w += self->item_h;
            self->text_x += self->item_h;
        }
    }

    if (!w) w = 10;
    if (!allitems_h) {
        allitems_h = 3;
        h += 3;
    }

    XResizeWindow(ob_display, self->window, w, h);
    XResizeWindow(ob_display, self->items, w, allitems_h);

    self->inner_w = w;

    if (!self->parent && self->show_title) {
        XResizeWindow(ob_display, self->title,
                      w, self->title_h - ob_rr_theme->bwidth);
        RrPaint(self->a_title, self->title,
                w, self->title_h - ob_rr_theme->bwidth);
        XMapWindow(ob_display, self->title);
    } else
        XUnmapWindow(ob_display, self->title);

    RrPaint(self->a_items, self->items, w, allitems_h);

    for (it = self->entries; it; it = g_list_next(it))
        menu_entry_frame_render(it->data);

    w += ob_rr_theme->bwidth * 2;
    h += ob_rr_theme->bwidth * 2;

    RECT_SET_SIZE(self->area, w, h);
}

static void menu_frame_update(ObMenuFrame *self)
{
    GList *mit, *fit;

    menu_find_submenus(self->menu);

    self->selected = NULL;

    for (mit = self->menu->entries, fit = self->entries; mit && fit;
         mit = g_list_next(mit), fit = g_list_next(fit))
    {
        ObMenuEntryFrame *f = fit->data;
        f->entry = mit->data;
    }

    while (mit) {
        ObMenuEntryFrame *e = menu_entry_frame_new(mit->data, self);
        self->entries = g_list_append(self->entries, e);
        mit = g_list_next(mit);
    }
    
    while (fit) {
        GList *n = g_list_next(fit);
        menu_entry_frame_free(fit->data);
        self->entries = g_list_delete_link(self->entries, fit);
        fit = n;
    }

    menu_frame_render(self);
}

void menu_frame_show(ObMenuFrame *self, ObMenuFrame *parent)
{
    GList *it;

    if (g_list_find(menu_frame_visible, self))
        return;

    if (parent) {
        if (parent->child)
            menu_frame_hide(parent->child);
        parent->child = self;
    }
    self->parent = parent;

    if (menu_frame_visible == NULL) {
        /* no menus shown yet */
        grab_pointer(TRUE, None);
        grab_keyboard(TRUE);
    }

    /* determine if the underlying menu is already visible */
    for (it = menu_frame_visible; it; it = g_list_next(it)) {
        ObMenuFrame *f = it->data;
        if (f->menu == self->menu)
            break;
    }
    if (!it) {
        if (self->menu->update_func)
            self->menu->update_func(self, self->menu->data);
    }

    menu_frame_visible = g_list_prepend(menu_frame_visible, self);
    menu_frame_update(self);

    menu_frame_move_on_screen(self);

    XMapWindow(ob_display, self->window);
}

void menu_frame_hide(ObMenuFrame *self)
{
    GList *it = g_list_find(menu_frame_visible, self);

    if (!it)
        return;

    menu_frame_visible = g_list_delete_link(menu_frame_visible, it);

    if (self->child)
        menu_frame_hide(self->child);

    if (self->parent)
        self->parent->child = NULL;
    self->parent = NULL;

    if (menu_frame_visible == NULL) {
        /* last menu shown */
        grab_pointer(FALSE, None);
        grab_keyboard(FALSE);
    }

    XUnmapWindow(ob_display, self->window);

    menu_frame_free(self);
}

void menu_frame_hide_all()
{
    GList *it = g_list_last(menu_frame_visible);
    if (it) 
        menu_frame_hide(it->data);
}

void menu_frame_hide_all_client(ObClient *client)
{
    GList *it = g_list_last(menu_frame_visible);
    if (it) {
        ObMenuFrame *f = it->data;
        if (f->client == client)
            menu_frame_hide(f);
    }
}


ObMenuFrame* menu_frame_under(gint x, gint y)
{
    ObMenuFrame *ret = NULL;
    GList *it;

    for (it = menu_frame_visible; it; it = g_list_next(it)) {
        ObMenuFrame *f = it->data;

        if (RECT_CONTAINS(f->area, x, y)) {
            ret = f;
            break;
        }
    }
    return ret;
}

ObMenuEntryFrame* menu_entry_frame_under(gint x, gint y)
{
    ObMenuFrame *frame;
    ObMenuEntryFrame *ret = NULL;
    GList *it;

    if ((frame = menu_frame_under(x, y))) {
        x -= ob_rr_theme->bwidth + frame->area.x;
        y -= frame->title_h + ob_rr_theme->bwidth + frame->area.y;

        for (it = frame->entries; it; it = g_list_next(it)) {
            ObMenuEntryFrame *e = it->data;

            if (RECT_CONTAINS(e->area, x, y)) {
                ret = e;            
                break;
            }
        }
    }
    return ret;
}

void menu_frame_select(ObMenuFrame *self, ObMenuEntryFrame *entry)
{
    ObMenuEntryFrame *old = self->selected;
    ObMenuFrame *oldchild = self->child;

    if (old == entry) return;

    if (entry && entry->entry->type != OB_MENU_ENTRY_TYPE_SEPARATOR)
        self->selected = entry;
    else
        self->selected = NULL;

    if (old)
        menu_entry_frame_render(old);
    if (oldchild)
        menu_frame_hide(oldchild);

    if (self->selected) {
        menu_entry_frame_render(self->selected);

        if (self->selected->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
            menu_entry_frame_show_submenu(self->selected);
    }
}

void menu_entry_frame_show_submenu(ObMenuEntryFrame *self)
{
    ObMenuFrame *f;

    if (!self->entry->data.submenu.submenu) return;

    f = menu_frame_new(self->entry->data.submenu.submenu,
                       self->frame->client);
    menu_frame_move(f,
                    self->frame->area.x + self->frame->area.width
                    - ob_rr_theme->menu_overlap - ob_rr_theme->bwidth,
                    self->frame->area.y + self->frame->title_h +
                    self->area.y + ob_rr_theme->menu_overlap);
    menu_frame_show(f, self->frame);
}

void menu_entry_frame_execute(ObMenuEntryFrame *self, gboolean hide)
{
    if (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
        self->entry->data.normal.enabled)
    {
        /* grab all this shizzle, cuz when the menu gets hidden, 'self'
           gets freed */
        ObMenuEntry *entry = self->entry;
        ObMenuExecuteFunc func = self->frame->menu->execute_func;
        gpointer data = self->frame->menu->data;
        GSList *acts = self->entry->data.normal.actions;
        ObClient *client = self->frame->client;

        /* release grabs before executing the shit */
        menu_frame_hide_all();

        if (func)
            func(entry, data);
        else {
            GSList *it;

            for (it = acts; it; it = g_slist_next(it))
            {
                ObAction *act = it->data;
                act->data.any.c = client;
                act->func(&act->data);
            }
        }
    }
}

void menu_frame_select_previous(ObMenuFrame *self)
{
    GList *it = NULL, *start;

    if (self->entries) {
        start = it = g_list_find(self->entries, self->selected);
        while (TRUE) {
            ObMenuEntryFrame *e;

            it = it ? g_list_previous(it) : g_list_last(self->entries);
            if (it == start)
                break;

            if (it) {
                e = it->data;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
                    break;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
                    e->entry->data.normal.enabled)
                    break;
            }
        }
    }
    menu_frame_select(self, it ? it->data : NULL);
}

void menu_frame_select_next(ObMenuFrame *self)
{
    GList *it = NULL, *start;

    if (self->entries) {
        start = it = g_list_find(self->entries, self->selected);
        while (TRUE) {
            ObMenuEntryFrame *e;

            it = it ? g_list_next(it) : self->entries;
            if (it == start)
                break;

            if (it) {
                e = it->data;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
                    break;
                if (e->entry->type == OB_MENU_ENTRY_TYPE_NORMAL &&
                    e->entry->data.normal.enabled)
                    break;
            }
        }
    }
    menu_frame_select(self, it ? it->data : NULL);
}