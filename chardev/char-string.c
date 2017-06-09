/*
 * QEMU Char Device for string input
 *
 * Copyright (c) 2017 Red Hat, Inc.
 *
 * Author: Dr. David Alan Gilbert <dgilbert@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include <stdint.h>
#include "qemu/option.h"
#include "qemu-common.h"
#include "chardev/char.h"
#include "qapi/error.h"

#define BUF_SIZE 32

typedef struct {
    Chardev parent;
    Chardev *outputdev;
    const char *text;
    const char *cur;
} StringChardev;

#define TYPE_CHARDEV_STRING "chardev-string"
#define STRING_CHARDEV(obj)                                    \
    OBJECT_CHECK(StringChardev, (obj), TYPE_CHARDEV_STRING)

static int string_chr_write(Chardev *chr, const uint8_t *buf, int len)
{
    //StringChardev *string = STRING_CHARDEV(chr);
    int orig_len = len;

    len = write(2, buf, len);
    fprintf(stderr, "%s: %d gives %d\n", __func__, orig_len, len);
    return len;
}

/* Tsk - sync is only used in a few weird cases - need a handler? Or can _read ? */
static int string_chr_sync_read(Chardev *chr, const uint8_t *buf, int len)
{
    StringChardev *stcd = STRING_CHARDEV(chr);
    size_t remaining_len;

    qemu_mutex_lock(&chr->chr_write_lock);
    remaining_len = strlen(stcd->cur);

    if (len > remaining_len) {
        len = remaining_len;
    }
    memcpy((void *)buf, stcd->cur, len);
    stcd->cur += len;

    qemu_mutex_unlock(&chr->chr_write_lock);
    fprintf(stderr, "%s: Giving %d bytes\n", __func__,len);
    return len;
}

static void qemu_chr_open_string(Chardev *chr,
                                 ChardevBackend *backend,
                                 bool *be_opened,
                                 Error **errp)
{
    ChardevString *cds = backend->u.string.data;
    StringChardev *stcd = STRING_CHARDEV(chr);

    stcd->text = cds->text;
    stcd->cur = stcd->text;
    /* TODO: Deal with outputdev */

    *be_opened = true;
}

static void qemu_chr_parse_string(QemuOpts *opts, ChardevBackend *backend,
                                  Error **errp)
{
    ChardevString *string;
    const char *option;

    string = backend->u.string.data = g_new0(ChardevString, 1);
    backend->type = CHARDEV_BACKEND_KIND_STRING;
    qemu_chr_parse_common(opts, qapi_ChardevString_base(string));
    
    option = qemu_opt_get(opts, "text");
    if (option) {
        string->text = strdup(option);
    } else {
        error_setg(errp, "chardev: No text given for string chardev");
        return;
    }
    option = qemu_opt_get(opts, "outputdev");
    if (option) {
        string->has_outputdev = true;
        string->outputdev = strdup(option);
    }
    
}

static void char_string_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->chr_write = string_chr_write;
    cc->chr_sync_read = string_chr_sync_read;
    cc->open = qemu_chr_open_string;
    cc->parse = qemu_chr_parse_string;
    /* Maybe try using the chr_add_watch or chr_update_read_handler */
    /* COuld use g_source_new like spice.c ? But whatabout output? */
}

static const TypeInfo char_string_type_info = {
    .name = TYPE_CHARDEV_STRING,
    .parent = TYPE_CHARDEV,
    .instance_size = sizeof(StringChardev),
    .class_init = char_string_class_init,
};

static void register_types(void)
{
    type_register_static(&char_string_type_info);
}

type_init(register_types);
