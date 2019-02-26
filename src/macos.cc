#if defined(__APPLE__)

#include <uv.h>
#include <nan.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/kern_event.h>
#include <sys/socket.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <ctype.h>
#include <fcntl.h>

using namespace v8;

#define UTUN_CONTROL_NAME "com.apple.net.utun_control"
#define UTUN_OPT_IFNAME 2

struct tun_t
{
  int fd;
  char ifname[10];
};

static void poll_worker(uv_poll_t *req, int status, int events)
{
  Nan::HandleScope scope;
  Nan::Callback *callback = (Nan::Callback *)req->data;
  callback->Call(0, 0);
}

static int open_tun_socket(tun_t *res)
{
  struct sockaddr_ctl addr;
  struct ctl_info info;

  res->fd = -1;
  memset(&res->ifname, 0, sizeof(res->ifname));
  socklen_t ifname_len = sizeof(res->ifname);

  int err = 0;

  res->fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  if (res->fd < 0)
    return res->fd;

  memset(&info, 0, sizeof(info));
  strncpy(info.ctl_name, UTUN_CONTROL_NAME, MAX_KCTL_NAME);

  err = ioctl(res->fd, CTLIOCGINFO, &info);
  if (err != 0)
    goto on_error;

  addr.sc_len = sizeof(addr);
  addr.sc_family = AF_SYSTEM;
  addr.ss_sysaddr = AF_SYS_CONTROL;
  addr.sc_id = info.ctl_id;
  addr.sc_unit = 0;

  err = connect(res->fd, (struct sockaddr *)&addr, sizeof(addr));
  if (err != 0)
    goto on_error;

  err = getsockopt(res->fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, res->ifname, &ifname_len);
  if (err != 0)
    goto on_error;

  err = fcntl(res->fd, F_SETFL, O_NONBLOCK);
  if (err != 0)
    goto on_error;

  fcntl(res->fd, F_SETFD, FD_CLOEXEC);
  if (err != 0)
    goto on_error;

on_error:
  if (err != 0)
  {
    close(res->fd);
    return err;
  }

  return res->fd;
}

NAN_METHOD(Tun)
{
  struct tun_t tunnel;
  int res = open_tun_socket(&tunnel);
  if (res < 0)
  {
    Nan::ThrowError("Could not open tun device");
    return;
  }
  uv_poll_t *handle = (uv_poll_t *)malloc(sizeof(uv_poll_t));
  Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());

  handle->data = callback;
  uv_poll_init(uv_default_loop(), handle, tunnel.fd);
  uv_poll_start(handle, UV_READABLE, poll_worker);

  Local<Object> result = Nan::New<Object>();

  result->Set(Nan::New<String>("fd").ToLocalChecked(), Nan::New<Number>(tunnel.fd));
  result->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(tunnel.ifname).ToLocalChecked());

  info.GetReturnValue().Set(result);
}

NAN_MODULE_INIT(Init)
{
  Nan::Set(target, Nan::New<String>("tun").ToLocalChecked(), Nan::New<FunctionTemplate>(Tun)->GetFunction());
}

NODE_MODULE(tun, Init)

#endif
