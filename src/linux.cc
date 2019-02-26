#if defined(__linux__)

#include <uv.h>
#include <nan.h>

#include <endian.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <regex>
#include <set>
#include <string>
#include <vector>

using namespace v8;

#define TUNTAP_DFT_PATH "/dev/net/tun"
#define TUNTAP_DFT_MTU 1500
// #define TUNTAP_DFT_PERSIST true
// #define TUNTAP_DFT_UP true
// #define TUNTAP_DFT_RUNNING true

struct tun_t
{
  int fd;
  char ifname[10];
};

static int min(int lhs, int rhs)
{
  return lhs < rhs ? lhs : rhs;
}

static void poll_worker(uv_poll_t *req, int status, int events)
{
  Nan::HandleScope scope;
  Nan::Callback *callback = (Nan::Callback *)req->data;
  callback->Call(0, 0);
}

static std::string get_interface_name()
{
  std::vector<std::string> names;
  struct ifaddrs *addrs, *tmp;

  getifaddrs(&addrs);
  tmp = addrs;

  while (tmp)
  {
    names.push_back(std::string(tmp->ifa_name));
    tmp = tmp->ifa_next;
  }

  freeifaddrs(addrs);

  std::regex rx("^tun([[:digit:]]+)$");
  std::set<int> indices;
  for (auto i = names.begin(); i != names.end(); ++i)
  {
    const std::string s = *i;

    std::smatch match;
    if (std::regex_search(s.begin(), s.end(), match, rx))
    {
      int idx = std::stoi(match[1], nullptr, 10);
      indices.insert(idx);
    }
  }

  for (int idx = 0;; ++idx)
  {
    if (indices.find(idx) == indices.end())
    {
      std::ostringstream name;
      name << "tun" << idx;
      return name.str();
    }
  }
}

static int open_tun_socket(tun_t *res)
{
  struct ifreq ifr;
  int tun_sock;

  res->fd = -1;
  memset(&res->ifname, 0, sizeof(res->ifname));
  socklen_t ifname_len = sizeof(res->ifname);

  res->fd = ::open(TUNTAP_DFT_PATH, O_RDWR);
  if (res->fd < 0)
    return -1;

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags |= IFF_TUN;
  strcpy(ifr.ifr_name, get_interface_name().c_str());

  int err = 0;

  err = ioctl(res->fd, TUNSETIFF, &ifr);
  if (err < 0)
  {
    err = -2;
    goto on_error;
  }

  strncpy(res->ifname, ifr.ifr_name, min(ifname_len, strlen(ifr.ifr_name)));

  err = ioctl(res->fd, TUNSETPERSIST, 0);
  if (err < 0)
  {
    err = -3;
    goto on_error;
  }

  tun_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (tun_sock < 0)
  {
    err = -4;
    goto on_error;
  }

  ifr.ifr_mtu = TUNTAP_DFT_MTU;
  ioctl(tun_sock, SIOCSIFMTU, &ifr);

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  ioctl(tun_sock, SIOCSIFFLAGS, &ifr);

  err = fcntl(res->fd, F_SETFL, O_NONBLOCK);
  if (err < 0)
  {
    err = -5;
    goto on_error;
  }

  fcntl(res->fd, F_SETFD, FD_CLOEXEC);
  if (err != 0)
  {
    err = -6;
    goto on_error;
  }

  ::close(tun_sock);

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
    char err[40] = "";
    sprintf(err, "Could not open tun device %d", res);
    Nan::ThrowError(err);
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
