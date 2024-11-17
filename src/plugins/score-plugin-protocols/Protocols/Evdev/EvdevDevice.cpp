#if defined(OSSIA_PROTOCOL_EVDEV)
#include "EvdevDevice.hpp"

#include "EvdevEvents.cpp"
#include "EvdevSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/dylib_loader.hpp>
#include <ossia/detail/timer.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/range/adaptor/sliced.hpp>

#include <QDebug>

#include <linux/input.h>
#include <sys/poll.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::EvdevDevice)
namespace ossia::net
{

struct evdev_protocol : public ossia::net::protocol_base
{
public:
  evdev_protocol(
      const Protocols::EvdevSpecificSettings& set, ossia::net::network_context_ptr ctx)
      : protocol_base{flags{}}
      , m_context{std::move(ctx)}
      , m_stream(
            m_context->context,
            open(
                QString("/dev/input/%1").arg(set.handler).toStdString().c_str(),
                O_RDONLY | O_NONBLOCK))
  {
  }

  ~evdev_protocol() { m_stream.close(); }

  static inline const char* codename(unsigned int type, unsigned int code)
  {
    using namespace evdev_input_names;
    return (type <= EV_MAX && code <= unsigned(maxval[type]) && names[type]
            && names[type][code])
               ? names[type][code]
               : "?";
  }

  void set_device(ossia::net::device_base& dev) override
  {
    m_device = &dev;

    auto& root = m_device->get_root_node();

    params.event = ossia::create_parameter(root, "/event", "list");

    auto info = this->device_info(m_stream.native_handle());
    for(const auto& type : info)
    {
      ossia::net::node_base* node{};
      switch(type.type)
      {
        case EV_KEY: {
          node = &ossia::net::create_node(root, "/event/key");
          params.key = node->create_parameter(ossia::val_type::INT);
          auto& k = params.categorized[EV_KEY];
          for(int code : type.event_codes)
            k.emplace(
                code, ossia::create_parameter(*node, codename(EV_KEY, code), "int"));
          break;
        }
        case EV_REL: {
          node = &ossia::net::create_node(root, "/event/relative");
          auto& k = params.categorized[EV_REL];

          for(int code : type.event_codes)
            k.emplace(
                code, ossia::create_parameter(*node, codename(EV_REL, code), "int"));
          break;
        }
        case EV_ABS: {
          node = &ossia::net::create_node(root, "/event/absolute");
          auto& k = params.categorized[EV_ABS];
          for(int code : type.event_codes)
            k.emplace(
                code, ossia::create_parameter(*node, codename(EV_ABS, code), "int"));
          break;
        }
        case EV_MSC: {
          node = &ossia::net::create_node(root, "/event/misc");
          auto& k = params.categorized[EV_MSC];
          for(int code : type.event_codes)
            k.emplace(
                code, ossia::create_parameter(*node, codename(EV_MSC, code), "int"));
          break;
        }
        case EV_SW: {
          node = &ossia::net::create_node(root, "/event/switch");
          auto& k = params.categorized[EV_SW];
          for(int code : type.event_codes)
            k.emplace(
                code, ossia::create_parameter(*node, codename(EV_SW, code), "int"));
          break;
        }
      }
    }
    post_read();
  }

  struct supported_event_type
  {
    unsigned int type{};
    std::vector<int> event_codes;
  };

  static std::vector<supported_event_type> device_info(int fd)
  {
    std::vector<supported_event_type> ret;

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x) ((x) % BITS_PER_LONG)
#define BIT(x) (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

    unsigned long bit[EV_MAX][NBITS(KEY_MAX)];

    // Read device metadata
    int version;
    if(ioctl(fd, EVIOCGVERSION, &version))
    {
      return {};
    }

    unsigned short id[4];
    ioctl(fd, EVIOCGID, id);

    char name[256] = "Unknown";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    memset(bit, 0, sizeof(bit));
    ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);

    // Look for all the supported events
    for(unsigned int type = 0; type < EV_MAX; type++)
    {
      if(test_bit(type, bit[0]) && type != EV_REP)
      {
        //int have_state = (get_state(fd, type, state, sizeof(state)) == 0);
        if(type == EV_SYN)
          continue;
        supported_event_type t{.type = type, .event_codes = {}};

        ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit[type]);
        for(unsigned int code = 0; code < KEY_MAX; code++)
          if(test_bit(code, bit[type]))
            t.event_codes.push_back(code);
        ret.push_back(std::move(t));
      }
    }

    return ret;
  }

  void push_to_param(int type, int code)
  {

    if(auto k_it = this->params.categorized.find(type);
       k_it != this->params.categorized.end())
    {
      auto& k = *k_it;
      if(auto c_it = k.second.find(code); c_it != k.second.end())
      {
        c_it->second->push_value(ossia::impulse{});
      }
    }
  }
  void push_to_param(int type, int code, int value)
  {
    if(auto k_it = this->params.categorized.find(type);
       k_it != this->params.categorized.end())
    {
      auto& k = *k_it;
      if(auto c_it = k.second.find(code); c_it != k.second.end())
      {
        c_it->second->push_value(value);
      }
    }
  }

  void post_read()
  {
    events.resize(32);
    m_stream.async_read_some(
        boost::asio::buffer(events),
        [this](boost::system::error_code ec, size_t bytes_transferred) {
      if(ec)
        return;

      auto const n = bytes_transferred / sizeof(::input_event);
      for(auto& ev : events | boost::adaptors::sliced(0, n))
      {
        switch(ev.type)
        {
          case EV_SYN:
            break;
          case EV_KEY: // raw keyboard input
            this->params.event->push_value(
                std::vector<ossia::value>{ev.type, ev.code, ev.value});
            if(this->params.key)
              this->params.key->push_value(ev.code);
            push_to_param(ev.type, ev.code, ev.value);
            break;

          case EV_REL: // relative, mouse-like
            this->params.event->push_value(
                std::vector<ossia::value>{ev.type, ev.code, ev.value});
            push_to_param(ev.type, ev.code, ev.value);
            break;

          case EV_ABS: // absolute, tablet-like
            this->params.event->push_value(
                std::vector<ossia::value>{ev.type, ev.code, ev.value});
            push_to_param(ev.type, ev.code, ev.value);
            break;
          case EV_MSC: // misc
            this->params.event->push_value(
                std::vector<ossia::value>{ev.type, ev.code, ev.value});
            push_to_param(ev.type, ev.code, ev.value);
            break;
          case EV_SW: // switch
            this->params.event->push_value(
                std::vector<ossia::value>{ev.type, ev.code, ev.value});
            push_to_param(ev.type, ev.code, ev.value);
            break;
          case EV_LED:
            break;
          case EV_SND:
            break;
          case EV_REP:
            break;
          case EV_FF:
            break;
          case EV_PWR:
            break;
        }
      }

      std::cout << "\n";

      post_read();
    });
  }

  bool pull(parameter_base& v) override { return false; }

  bool push(const parameter_base& p, const value& v) override { return false; }
  bool push_raw(const full_parameter_data&) override { return false; }
  bool observe(parameter_base&, bool) override { return false; }
  bool update(node_base& node_base) override { return false; }

  ossia::net::network_context_ptr m_context;
  ossia::net::device_base* m_device{};
  struct
  {
    ossia::net::parameter_base* event{};
    ossia::net::parameter_base* key{};
    ossia::net::parameter_base* mouse_xy{};
    ossia::flat_map<int, ossia::flat_map<int, ossia::net::parameter_base*>> categorized;
  } params;

  boost::asio::posix::stream_descriptor m_stream;
  std::vector<::input_event> events;
};
}
namespace Protocols
{

EvdevDevice::EvdevDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

EvdevDevice::~EvdevDevice() { }

bool EvdevDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set = m_settings.deviceSpecificSettings.value<EvdevSpecificSettings>();
    {
      auto pproto = std::make_unique<ossia::net::evdev_protocol>(set, m_ctx);
      auto dev = std::make_unique<ossia::net::generic_device>(
          std::move(pproto), settings().name.toStdString());
      m_dev = std::move(dev);
    }
    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::runtime_error& e)
  {
    qDebug() << "Evdev error: " << e.what();
  }
  catch(...)
  {
    qDebug() << "Evdev error";
  }

  return connected();
}

void EvdevDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
#endif