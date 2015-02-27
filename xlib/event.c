_Bool doevent(XEvent event)
{
    if(XFilterEvent(&event, None)) return 1;
    if(event.xany.window && event.xany.window != window) {
        if(event.type == ClientMessage) {
            XClientMessageEvent *ev = &event.xclient;
            if((Atom)event.xclient.data.l[0] == wm_delete_window) {
                if(ev->window == video_win[0]) {
                    video_end(0);
                    video_preview = 0;
                    toxvideo_postmessage(VIDEO_PREVIEW_END, 0, 0, NULL);
                    return 1;
                }

                int i;
                for(i = 0; i != countof(friend); i++) {
                    if(video_win[i + 1] == ev->window) {
                        FRIEND *f = &friend[i];
                        tox_postmessage(TOX_HANGUP, f->callid, 0, NULL);
                        break;
                    }
                }
                if(i == countof(friend)) {
                    debug("this should not happen\n");
                }
            }
        }
        return 1;
    }

    switch(event.type) {
    case Expose: {
        enddraw_common(0, 0, 0, utox_window_width, utox_window_height);
        debug("expose\n");
        break;
    }

    case FocusIn: {
        if (xic) {
            XSetICFocus(xic);
        }

        havefocus = 1;
        XWMHints hints = {0};
        XSetWMHints(display, window, &hints);
        break;
    }

    case FocusOut: {
        if (xic) {
            XUnsetICFocus(xic);
        }

        havefocus = 0;
        break;
    }

    case ConfigureNotify: {
        XConfigureEvent *ev = &event.xconfigure;
        if(utox_window_width != ev->width || utox_window_height != ev->height) {
            debug("resize\n");

            if(ev->width > drawwidth || ev->height > drawheight) {
                drawwidth = ev->width + 10;
                drawheight = ev->height + 10;

                XFreePixmap(display, drawbuf);
                drawbuf = XCreatePixmap(display, window, drawwidth, drawheight, 24);
                XRenderFreePicture(display, renderpic);
                renderpic = XRenderCreatePicture(display, drawbuf,XRenderFindStandardFormat(display, PictStandardRGB24), 0, NULL);
            }

            utox_window_width = ev->width;
            utox_window_height = ev->height;

            ui_size(utox_window_width, utox_window_height);

            redraw();
        }

        break;
    }

    case LeaveNotify: {
        ui_mouseleave();
    }

    case MotionNotify: {
        XMotionEvent *ev = &event.xmotion;
        if(pointergrab) {
            XDrawRectangle(display, RootWindow(display, screen), grabgc, grabx < grabpx ? grabx : grabpx, graby < grabpy ? graby : grabpy,
                           grabx < grabpx ? grabpx - grabx : grabx - grabpx, graby < grabpy ? grabpy - graby : graby - grabpy);

            grabpx = ev->x_root;
            grabpy = ev->y_root;

            XDrawRectangle(display, RootWindow(display, screen), grabgc, grabx < grabpx ? grabx : grabpx, graby < grabpy ? graby : grabpy,
                           grabx < grabpx ? grabpx - grabx : grabx - grabpx, graby < grabpy ? grabpy - graby : graby - grabpy);

            break;
        }


        static int mx, my;
        int dx, dy;

        dx = ev->x - mx;
        dy = ev->y - my;
        mx = ev->x;
        my = ev->y;

        cursor = CURSOR_NONE;
        panel_mmove(&panel_main, 0, 0, 0, utox_window_width, utox_window_height, ev->x, ev->y, dx, dy);

        XDefineCursor(display, window, cursors[cursor]);

        //SetCursor(hand ? cursor_hand : cursor_arrow);

        //debug("MotionEvent: (%u %u) %u\n", ev->x, ev->y, ev->state);
        break;
    }

    case ButtonPress: {
        XButtonEvent *ev = &event.xbutton;
        switch(ev->button) {
        case Button2: {
            panel_mdown(&panel_main, 0);
            panel_mup(&panel_main, 0);

            pasteprimary();
            break;
        }

        case Button1: {
            if(pointergrab) {
                grabpx = grabx = ev->x_root;
                grabpy = graby = ev->y_root;

                //XDrawRectangle(display, RootWindow(display, screen), grabgc, grabx, graby, 0, 0);

                break;
            }

            //todo: better double/triple click detect
            static Time lastclick, lastclick2;
            panel_mmove(&panel_main, 0, 0, 0, utox_window_width, utox_window_height, ev->x, ev->y, 0, 0);
            panel_mdown(&panel_main, 0);
            if(ev->time - lastclick < 300) {
                _Bool triclick = (ev->time - lastclick2 < 600);
                panel_dclick(&panel_main, 0, triclick);
                if(triclick) {
                    lastclick = 0;
                }
            }
            lastclick2 = lastclick;
            lastclick = ev->time;
            mdown = 1;
            break;
        }

        case Button3: {
            panel_mright(&panel_main);
            break;
        }

        case Button4: {
            panel_mwheel(&panel_main, 0, 0, utox_window_width, utox_window_height, 1.0);
            break;
        }

        case Button5: {
            panel_mwheel(&panel_main, 0, 0, utox_window_width, utox_window_height, -1.0);
            break;
        }

        }

        //debug("ButtonEvent: %u %u\n", ev->state, ev->button);
        break;
    }

    case ButtonRelease: {
        XButtonEvent *ev = &event.xbutton;
        switch(ev->button) {
        case Button1: {
            if(pointergrab) {
                if(grabx < grabpx) {
                    grabpx -= grabx;
                } else {
                    int w = grabx - grabpx;
                    grabx = grabpx;
                    grabpx = w;
                }

                if(graby < grabpy) {
                    grabpy -= graby;
                } else {
                    int w = graby - grabpy;
                    graby = grabpy;
                    grabpy = w;
                }

                XDrawRectangle(display, RootWindow(display, screen), grabgc, grabx, graby, grabpx, grabpy);
                XUngrabPointer(display, CurrentTime);
                if(pointergrab == 1) {
                    FRIEND *f = sitem->data;
                    if(sitem->item == ITEM_FRIEND && f->online) {
                        XImage *img = XGetImage(display, RootWindow(display, screen), grabx, graby, grabpx, grabpy, XAllPlanes(), ZPixmap);
                        if(img) {
                            uint8_t *out;
                            size_t size;
                            uint8_t *temp, *p;
                            uint32_t *pp = (void*)img->data, *end = &pp[img->width * img->height];
                            p = temp = malloc(img->width * img->height * 3);
                            while(pp != end) {
                                uint32_t i = *pp++;
                                *p++ = i >> 16;
                                *p++ = i >> 8;
                                *p++ = i;
                            }
                            lodepng_encode_memory(&out, &size, temp, img->width, img->height, LCT_RGB, 8);
                            free(temp);

                            uint16_t w = img->width;
                            uint16_t h = img->height;

                            UTOX_NATIVE_IMAGE *image = malloc(sizeof(UTOX_NATIVE_IMAGE));
                            image->rgb = ximage_to_picture(img, NULL);
                            image->alpha = None;
                            friend_sendimage(f, image, w, h, (UTOX_PNG_IMAGE)out, size);
                        }
                    }
                } else {
                    toxvideo_postmessage(VIDEO_SET, 0, 0, (void*)1);
                }
                pointergrab = 0;
            } else {
                panel_mup(&panel_main, 0);
                mdown = 0;
            }
            break;
        }
        }
        break;
    }

    case KeyPress: {
        XKeyEvent *ev = &event.xkey;
        KeySym sym = XLookupKeysym(ev, 0);//XKeycodeToKeysym(display, ev->keycode, 0)

        wchar_t buffer[16];
        int len;

        if (xic) {
            len = XwcLookupString(xic, ev, buffer, sizeof(buffer), &sym, NULL);
        } else {
            len = XLookupString(ev, (char *)buffer, sizeof(buffer), &sym, NULL);
        }
        if(edit_active()) {
            if(ev->state & 4) {
                switch(sym) {
                case 'v':
                    paste();
                    return 1;
                case 'c':
                case XK_Insert:
                    copy(0);
                    return 1;
                case 'x':
                    copy(0);
                    edit_char(KEY_DEL, 1, 0);
                    return 1;
                case 'w':
                    edit_char(KEY_BACK, 1, 4);
                    return 1;
                }
            }

            if(ev->state & ShiftMask) {
                switch(sym) {
                    case XK_Insert:
                        paste();
                        return 1;
                    case XK_Delete:
                        copy(0);
                        edit_char(KEY_DEL, 1, 0);
                        return 1;
                }
            }

            if (sym == XK_KP_Enter){
                sym = XK_Return;
            }

            if (sym == XK_ISO_Left_Tab) {
                sym = XK_Tab;
            }

            if (sym == XK_Return && (ev->state & 1)) {
                edit_char('\n', 0, 0);
                break;
            }

            if (sym == XK_KP_Space) {
                sym = XK_space;
            }

            if (sym >= XK_KP_Home && sym <= XK_KP_Begin) {
                sym -= 0x45;
            }

            if (sym >= XK_KP_Multiply && sym <= XK_KP_Equal) {
                sym -= 0xFF80;
            }

            if (!sym) {
              int i;
              for(i = 0; i < len; i++)
                edit_char(buffer[i], (ev->state & 4) != 0, ev->state);
            }
            uint32_t key = keysym2ucs(sym);
            if(key != ~0) {
                edit_char(key, (ev->state & 4) != 0, ev->state);
            } else {
                edit_char(sym, 1, ev->state);
            }

            break;
        }

        messages_char(sym);

        if(ev->state & 4) {
            if(sym == 'c') {
                if(sitem->item == ITEM_FRIEND) {
                    clipboard.len = messages_selection(&messages_friend, clipboard.data, sizeof(clipboard.data), 1);
                    setclipboard();
                } else if(sitem->item == ITEM_GROUP) {
                    clipboard.len = messages_selection(&messages_group, clipboard.data, sizeof(clipboard.data), 1);
                    setclipboard();
                }
                break;
            }
        }

        if(sym == XK_Delete) {
            list_deletesitem();
        }

        break;
    }

    case SelectionNotify: {

        debug("SelectionNotify\n");

        XSelectionEvent *ev = &event.xselection;

        if(ev->property == None) {
            break;
        }

        Atom type;
        int format;
        long unsigned int len, bytes_left;
        void *data;

        XGetWindowProperty(display, window, ev->property, 0, ~0L, True, AnyPropertyType, &type, &format, &len, &bytes_left, (unsigned char**)&data);

        if(!data) {
            break;
        }

        debug("Type: %s\n", XGetAtomName(display, type));
        debug("Property: %s\n", XGetAtomName(display, ev->property));

        if(ev->property == XA_ATOM) {
            pastebestformat((Atom *)data, len, ev->selection);
        } else if(ev->property == XdndDATA) {
            char *path = malloc(len + 1);
            formaturilist(path, (char*)data, len);
            tox_postmessage(TOX_SENDFILES, (FRIEND*)sitem->data - friend, 0xFFFF, path);
        } else if (type == XA_INCR) {
            if (pastebuf.data) {
                /* already pasting something, give up on that */
                free(pastebuf.data);
                pastebuf.data = NULL;
            }
            pastebuf.len = *(unsigned long *)data;
            pastebuf.left = pastebuf.len;
            pastebuf.data = malloc(pastebuf.len);
            /* Deleting the window property triggers incremental paste */
        } else {
            pastedata(data, type, len, ev->selection == XA_PRIMARY);
        }

        XFree(data);

        break;
    }

    case SelectionRequest: {
        XSelectionRequestEvent *ev = &event.xselectionrequest;

        debug("SelectionRequest\n");

        XEvent resp = {
            .xselection = {
                .type = SelectionNotify,
                .property = ev->property,
                .requestor = ev->requestor,
                .selection = ev->selection,
                .target = ev->target,
                .time = ev->time
            }
        };//self.id, sizeof(self.id)

        if(ev->target == XA_UTF8_STRING || ev->target == XA_STRING) {
            if(ev->selection == XA_PRIMARY) {
                debug("%u\n", primary.len);
                XChangeProperty(display, ev->requestor, ev->property, ev->target, 8, PropModeReplace, primary.data, primary.len);
            } else {
                XChangeProperty(display, ev->requestor, ev->property, ev->target, 8, PropModeReplace, clipboard.data, clipboard.len);
            }
        } else if(ev->target == targets) {
            Atom supported[] = {XA_STRING, XA_UTF8_STRING};
            XChangeProperty(display, ev->requestor, ev->property, XA_ATOM, 32, PropModeReplace, (void*)&supported, countof(supported));
        }
        else {
            debug("unknown request\n");
            resp.xselection.property = None;
        }

        XSendEvent(display, ev->requestor, 0, 0, &resp);

        break;
    }

    case PropertyNotify:
        {
            XPropertyEvent *ev = &event.xproperty;
            if (ev->state == PropertyNewValue && ev->atom == targets && pastebuf.data) {
                debug("Property changed: %s\n", XGetAtomName(display, ev->atom));

                Atom type;
                int format;
                unsigned long int len, bytes_left;
                void *data;

                XGetWindowProperty(display, window, ev->atom, 0, ~0L, True, AnyPropertyType, &type, &format, &len, &bytes_left, (unsigned char**)&data);

                if (len == 0) {
                    debug("Got 0 length data, pasting\n");
                    pastedata(pastebuf.data, type, pastebuf.len, False);
                    pastebuf.data = NULL;
                    break;
                }

                if (pastebuf.left < len) {
                    pastebuf.len += len - pastebuf.left;
                    pastebuf.data = realloc(pastebuf.data, pastebuf.len);
                    pastebuf.left = len;
                }

                memcpy(pastebuf.data + pastebuf.len - pastebuf.left, data, len);
                pastebuf.left -= len;

                XFree(data);
            }
            break;
        }

    case ClientMessage:
        {
            XClientMessageEvent *ev = &event.xclient;
            if(ev->window == 0) {
                void *data;
                memcpy(&data, &ev->data.s[2], sizeof(void*));
                tox_message(ev->message_type, ev->data.s[0], ev->data.s[1], data);
                break;
            }

            if(ev->message_type == wm_protocols) {
                if((Atom)event.xclient.data.l[0] == wm_delete_window) {
                    return 0;
                }
                break;
            }

            if(ev->message_type == XdndEnter) {
                debug("enter\n");
            } else if(ev->message_type == XdndPosition) {
                Window src = ev->data.l[0];
                XEvent reply_event = {
                    .xclient = {
                        .type = ClientMessage,
                        .display = display,
                        .window = src,
                        .message_type = XdndStatus,
                        .format = 32,
                        .data = {
                            .l = {window, 1, 0, 0, XdndActionCopy}
                        }
                    }
                };

                XSendEvent(display, src, 0, 0, &reply_event);
                //debug("position (version=%u)\n", ev->data.l[1] >> 24);
            } else if(ev->message_type == XdndStatus) {
                debug("status\n");
            } else if(ev->message_type == XdndDrop) {
                XConvertSelection(display, XdndSelection, XA_STRING, XdndDATA, window, CurrentTime);
                debug("drop\n");
            } else if(ev->message_type == XdndLeave) {
                debug("leave\n");
            } else {
                debug("dragshit\n");
            }
            break;
        }

    }

    return 1;
}
