#include "config.h"

#include <errno.h>
#include <sys/select.h>
#include <unistd.h>

#include "utils.h"
#include "vty.h"
#include "vty_cli.h"
#include "vty_utils.h"

/* if this code looks like quagga lib/vty.c it is because we stole it in good part */

#define CONTROL(X)	((X) - '@')
#define VTY_NORMAL	0
#define VTY_PRE_ESCAPE	1
#define VTY_ESCAPE	2

static const char telnet_backward_char[] = { 0x08 };
static const char telnet_newline[] = { '\n', '\r', 0x0 };

static void knet_vty_reset_buf(struct knet_vty *vty)
{
	memset(vty->line, 0, sizeof(vty->line));
	vty->line_idx = 0;
	vty->cursor_pos = 0;
}

static void knet_vty_add_to_buf(struct knet_vty *vty, unsigned char *buf, int pos)
{
	if (vty->cursor_pos == vty->line_idx) {
		vty->line[vty->line_idx] = buf[pos];
	} else {
		memmove(&vty->line[vty->cursor_pos+1], &vty->line[vty->cursor_pos],
			vty->line_idx - vty->cursor_pos);
		vty->line[vty->cursor_pos] = buf[pos];
	}
	vty->line_idx++;
	vty->cursor_pos++;
}

static void knet_vty_rewrite_line(struct knet_vty *vty)
{
	int i;

	for (i = 0; i <= vty->cursor_pos; i++)
		knet_vty_write(vty, "%s", telnet_backward_char);

	knet_vty_write(vty, "%s", vty->line);

	for (i = 0; i < (vty->line_idx - vty->cursor_pos); i++)
		knet_vty_write(vty, "%s", telnet_backward_char);

}

static void knet_vty_forward_char(struct knet_vty *vty)
{
	char buf[2];

	if (vty->cursor_pos < vty->line_idx) {
		buf[1] = vty->line[vty->cursor_pos];
		buf[2] = 0;
		knet_vty_write(vty, "%s", buf);
		vty->cursor_pos++;
	}
}

static void knet_vty_backward_char(struct knet_vty *vty)
{
	if (vty->cursor_pos > 0) {
		knet_vty_write(vty, "%s", telnet_backward_char);
		vty->cursor_pos--;
	}
}

static void knet_vty_kill_line(struct knet_vty *vty)
{
	int size, i;

	size = vty->line_idx - vty->cursor_pos;

	if (size == 0)
		return;

	for (i = 0; i < size; i++)
		knet_vty_write(vty, " ");

	for (i = 0; i < size; i++)
		knet_vty_write(vty, "%s", telnet_backward_char);

	memset(&vty->line[vty->cursor_pos], 0, size);
	vty->line_idx = vty->cursor_pos;
}

static void knet_vty_delete_char(struct knet_vty *vty)
{
	int size, i;

	if (vty->line_idx == 0)
		log_info("Write function to go one level down");

	if (vty->line_idx == vty->cursor_pos)
		return;

	size = vty->line_idx - vty->cursor_pos;

	vty->line_idx--;
	memmove(&vty->line[vty->cursor_pos], &vty->line[vty->cursor_pos+1],
		size - 1);
	vty->line[vty->line_idx] = '\0';

	knet_vty_write(vty, "%s ", &vty->line[vty->cursor_pos]);
	for (i = 0; i < size; i++)
		knet_vty_write(vty, "%s", telnet_backward_char);
}

static void knet_vty_delete_backward_char(struct knet_vty *vty)
{
	if (vty->cursor_pos == 0)
		return;

	knet_vty_backward_char(vty);
	knet_vty_delete_char(vty);
}

static int knet_vty_process_buf(struct knet_vty *vty, unsigned char *buf, int buflen)
{
	int i;

	if (vty->line_idx >= KNET_VTY_MAX_LINE)
		return -1;

	for (i = 0; i < buflen; i++) {
		if (vty->escape == VTY_ESCAPE) {
			switch (buf[i]) {
				case ('A'):
					log_info("previous line");
					break;
				case ('B'):
					log_info("next line");
					break;
				case ('C'):
					knet_vty_forward_char(vty);
					break;
				case ('D'):
					knet_vty_backward_char(vty);
					break;
				default:
					break;
			}
			vty->escape = VTY_NORMAL;
			continue;
		}

		if (vty->escape == VTY_PRE_ESCAPE) {
			switch (buf[i]) {
				case '[':
					vty->escape = VTY_ESCAPE;
					break;
				case 'b':
					vty->escape = VTY_NORMAL;
					log_info("backword word");
					break;
				case 'f':
					vty->escape = VTY_NORMAL;
					log_info("forward word");
					break;
				case 'd':
					vty->escape = VTY_NORMAL;
					log_info("forward kill word");
					break;
				case CONTROL('H'):
				case 0x7f:
					vty->escape = VTY_NORMAL;
					log_info("backward kill word");
					break;
				default:
					break;
			}
			continue;
		}

		switch (buf[i]) {
			case CONTROL('A'):
				while (vty->cursor_pos != 0)
					knet_vty_backward_char(vty);
				break;
			case CONTROL('B'):
				knet_vty_backward_char(vty);
				break;
			case CONTROL('C'):
				knet_vty_write(vty, "%s", telnet_newline);
				knet_vty_reset_buf(vty);
				break;
			case CONTROL('D'):
				knet_vty_delete_char(vty);
				break;
			case CONTROL('E'):
				while (vty->cursor_pos != vty->line_idx)
					knet_vty_forward_char(vty);
				break;
			case CONTROL('F'):
				knet_vty_forward_char(vty);
				break;
			case CONTROL('H'):
			case 0x7f:
				knet_vty_delete_backward_char(vty);
				break;
			case CONTROL('K'):
				knet_vty_kill_line(vty);
				break;
			case CONTROL('N'):
				log_info("next line");
				break;
			case CONTROL('P'):
				log_info("previous line");
				break;
			case CONTROL('T'):
				log_info("transport chars");
				break;
			case CONTROL('U'):
				log_info("kill line from beginning");
				break;
			case CONTROL('W'):
				log_info("kill backward word");
				break;
			case CONTROL('Z'):
				log_info("end config");
				break;
			case '\n':
			case '\r':
				knet_vty_write(vty, "%s", telnet_newline);
				if (strlen(vty->line))
					knet_vty_write(vty, "Processing: %s%s",
							vty->line, telnet_newline);
				knet_vty_reset_buf(vty);
				break;
			case '\t':
				log_info("command completion");
				break;
			case '?':
				log_info("help");
				break;
			case '\033':
				vty->escape = VTY_PRE_ESCAPE;
				break;
			default:
				if (buf[i] > 31 && buf[i] < 127)
					knet_vty_add_to_buf(vty, buf, i);
				break;
		}
	}

	return 0;
}

void knet_vty_cli_bind(struct knet_vty *vty)
{
	int se_result = 0;
	fd_set rfds;
	struct timeval tv;
	unsigned char buf[VTY_MAX_BUFFER_SIZE];
	int readlen;

	while (se_result >= 0 && !vty->got_epipe) {
		FD_ZERO (&rfds);
		FD_SET (vty->vty_sock, &rfds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		se_result = select((vty->vty_sock + 1), &rfds, 0, 0, &tv);

		if ((se_result == -1) || (vty->got_epipe))
			goto out_clean;

		if ((se_result == 0) || (!FD_ISSET(vty->vty_sock, &rfds)))
			continue;

		memset(buf, 0 , sizeof(buf));
		readlen = knet_vty_read(vty, buf, sizeof(buf));
		if (readlen <= 0)
			goto out_clean;

		if (knet_vty_process_buf(vty, buf, readlen) < 0) {
			knet_vty_write(vty, "\nError processing command: command too long\n");
			knet_vty_reset_buf(vty);
		}

		if (vty->escape == VTY_NORMAL)
			knet_vty_rewrite_line(vty);
	}

out_clean:

	return;
}