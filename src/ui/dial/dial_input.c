#include "dial_internal.h"

static void pointer(Dial *d, float px, float py, bool click, int *root, int *child) {
    float scale = d->scale*d->opening;
    float x = (px-(float)d->width/2)/scale, y = (py-(float)d->height/2)/scale;
    float radius = hypotf(x,y);
    /* Focus mode only holds while the pointer really sits on the child ring.
       Dropping it before the hit test keeps the other parents reachable even
       while a wheel-selected child is still pending. */
    if (d->child_focus && radius < 190) { d->child_focus = false; d->dirty = true; }
    dial_hit(d,x,y,root,child);
    /* A child picked with the wheel stays current while the pointer rests on
       its parent sector, so scrolling never has to chase the child ring. */
    if (*child < 0 && *root == d->active && d->child >= 0) *child = d->child;
    if (click && *child < 0 && *root >= 0) {
        float angle = atan2f(y,x)-(-DIAL_PI/2+(float)*root*2*DIAL_PI/(float)d->count);
        if (radius < 73 || radius > 198 ||
            fabsf(atan2f(sinf(angle),cosf(angle))) > DIAL_PI/(float)d->count) *root = -1;
    }
}

static void activate(Dial *d) {
    if (d->active < 0) return;
    char text[32];
    DialItem item = d->child < 0 ? d->items[d->active] : dial_child_item(d,d->child,text,sizeof(text));
    if (item.children) { dial_select(d,d->active,0); return; }
    if (item.command != BONGO_CAT_MENU_NONE) { d->result = item.command; d->done = true; }
}

/* Host callbacks render through the pet's context; hop over before the call
   and back to the menu surface afterwards, exactly like dial_preview(). There
   is nothing to hop back to before the menu owns a context. */
static void host_call(Dial *d, BongoCatMenuPreview callback, BongoCatMenuAction action) {
    if (!callback || action == BONGO_CAT_MENU_NONE) return;
    bool context = d->window && d->context;
    if (context && d->previous_window && d->previous_context)
        SDL_GL_MakeCurrent(d->previous_window,d->previous_context);
    callback(d->labels->preview_userdata,action);
    if (context && !SDL_GL_MakeCurrent(d->window,d->context)) d->done = true;
}

/* Left click on a parent or on one of its children confirms the pending child
   in place: the host pins the chosen group so it survives the eventual close,
   and the menu stays open for further changes. A parent without a pending
   child only opens its child ring. */
static void confirm(Dial *d) {
    if (d->active < 0) return;
    if (d->child < 0) { activate(d); return; }
    char text[32];
    host_call(d,d->labels->restore,dial_child_item(d,d->child,text,sizeof(text)).command);
}

/* Right click drops the pending child and rolls its preview back to the last
   confirmed state, leaving the menu open with the parent still hovered. */
static void cancel(Dial *d) {
    d->child = -1;
    d->child_focus = false;
    d->pressed = -1;
    if (d->preview != BONGO_CAT_MENU_NONE) dial_preview(d,BONGO_CAT_MENU_NONE);
    d->preview = BONGO_CAT_MENU_NONE;
    d->dirty = true;
}

static void page(Dial *d, int direction) {
    if (d->active < 0 || d->items[d->active].children <= DIAL_PAGE) return;
    int pages = ((int)d->items[d->active].children+DIAL_PAGE-1)/DIAL_PAGE;
    d->page = (d->page+direction+pages)%pages;
    d->child = -1;
    if (d->preview != BONGO_CAT_MENU_NONE) dial_preview(d,BONGO_CAT_MENU_NONE);
    d->preview = BONGO_CAT_MENU_NONE;
    d->changed_at = SDL_GetTicks();
    dial_child_paths(d); d->dirty = true;
}

/* The wheel walks the hovered parent's children instead of paging: one notch
   selects (and previews) one child. Crossing a page edge turns the page and
   keeps walking, so catalogs longer than one page stay reachable. */
static void wheel(Dial *d, int direction) {
    if (d->active < 0) return;
    int total = (int)d->items[d->active].children;
    int count = dial_child_count(d);
    if (total <= 0 || count <= 0) return;
    if (d->child < 0) {
        dial_select(d,d->active,direction > 0 ? 0 : count-1);
        return;
    }
    int next = d->child+direction;
    if (next >= 0 && next < count) { dial_select(d,d->active,next); return; }
    if (total <= DIAL_PAGE) { dial_select(d,d->active,(next+count)%count); return; }
    int pages = (total+DIAL_PAGE-1)/DIAL_PAGE;
    d->page = (d->page+direction+pages)%pages;
    d->changed_at = SDL_GetTicks();
    dial_child_paths(d);
    dial_select(d,d->active,direction > 0 ? 0 : dial_child_count(d)-1);
}

static void key(Dial *d, const SDL_KeyboardEvent *event) {
    SDL_Keycode value = event->key;
    if (value == SDLK_ESCAPE) {
        if (d->active >= 0 && dial_child_count(d)) dial_select(d,-1,-1);
        else d->done = true;
    } else if (value == SDLK_RETURN || value == SDLK_KP_ENTER || value == SDLK_SPACE) activate(d);
    else if (value == SDLK_PAGEUP || value == SDLK_PAGEDOWN) page(d,value == SDLK_PAGEDOWN ? 1 : -1);
    else if (value == SDLK_DOWN || value == SDLK_S) {
        if (d->child < 0 && dial_child_count(d)) dial_select(d,d->active,0);
    } else if (value == SDLK_UP || value == SDLK_W || value == SDLK_BACKSPACE) {
        d->child_focus = false; d->dirty = true; dial_select(d,d->active,-1);
    } else if (value == SDLK_LEFT || value == SDLK_RIGHT ||
        value == SDLK_A || value == SDLK_D || value == SDLK_TAB) {
        int direction = value == SDLK_RIGHT || value == SDLK_D ||
            (value == SDLK_TAB && (event->mod & SDL_KMOD_SHIFT)) ? -1 : 1;
        int count = dial_child_count(d);
        if (d->child >= 0 && count) dial_select(d,d->active,(d->child+direction+count)%count);
        else dial_select(d,d->active < 0 ? 0 : (d->active+direction+d->count)%d->count,-1);
    }
}

void dial_event(Dial *d, const SDL_Event *e) {
    int root, child;
    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION:
        pointer(d,e->motion.x,e->motion.y,false,&root,&child);
        dial_select(d,root,child); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button != SDL_BUTTON_LEFT) {
            /* Right click cancels the pending child in place; clicking away
               from the dial still dismisses the menu. */
            if (e->button.button == SDL_BUTTON_RIGHT) {
                pointer(d,e->button.x,e->button.y,true,&root,&child);
                if (root >= 0) { cancel(d); break; }
            }
            d->done = true; break;
        }
        pointer(d,e->button.x,e->button.y,true,&root,&child);
        if (root < 0) { d->done = true; break; }
        dial_select(d,root,child);
        d->pressed = child >= 0 ? DIAL_ROOTS+child : root; d->dirty = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (e->button.button != SDL_BUTTON_LEFT) break;
        int pressed = d->pressed; d->pressed = -1; d->dirty = true;
        pointer(d,e->button.x,e->button.y,true,&root,&child);
        if (root >= 0 && pressed == (child >= 0 ? DIAL_ROOTS+child : root)) {
            dial_select(d,root,child); confirm(d);
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        float delta = e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e->wheel.y : e->wheel.y;
        if (delta != 0) wheel(d,delta < 0 ? 1 : -1);
        break;
    }
    case SDL_EVENT_KEY_DOWN: key(d,&e->key); break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: dial_select(d,-1,-1); break;
    case SDL_EVENT_WINDOW_SHOWN: d->shown = true; d->dirty = true; break;
    case SDL_EVENT_WINDOW_HIDDEN:
        if (d->shown && (SDL_GetWindowFlags(d->window) & SDL_WINDOW_HIDDEN)) d->done = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        /* Creation/show may queue a transient loss before focus settles. */
        if (d->shown && SDL_GetKeyboardFocus() != d->window) d->done = true;
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: d->done = true; break;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_HDR_STATE_CHANGED: d->dirty = true; break;
    case SDL_EVENT_WINDOW_RESIZED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        if (!SDL_GetWindowSize(d->window,&d->width,&d->height) ||
            !SDL_GetWindowSizeInPixels(d->window,&d->pixel_width,&d->pixel_height) ||
            d->width < 1 || d->height < 1) { d->done = true; break; }
        d->scale = (float)SDL_min(d->width,d->height)/608.0f;
        d->raster_scale = (float)d->pixel_width/(float)d->width;
        /* SDL also sends DISPLAY_SCALE_CHANGED during window creation.
           Keep the menu open; existing textures remain valid and new covers
           use the updated density. Fonts are rebaked on the next opening. */
        d->dirty = true;
        break;
    }
}
