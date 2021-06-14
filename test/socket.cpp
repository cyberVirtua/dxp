#define BOOST_TEST_MODULE Sockets Test

#include "../src/dexpo_socket.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <random>
#include <string>
#include <thread>

// For random numbers in tests
#define MAX_NUM 10000000
#define SMALL_MAX_NUM 38000

bool
operator== (const dexpo_pixmap d1, const dexpo_pixmap d2)
{
  return d1.desktop_number == d2.desktop_number && d1.width == d2.width
         && d1.height == d2.height && d1.id == d2.id
         && strcmp (d1.name, d2.name) == 0;
};

void
operator<< (std::ostream &stream, const dexpo_pixmap &d)
{
  stream << d.name << ", " << d.id << ", " << d.width << ", " << d.height
         << ", " << d.desktop_number;
};

// To generate random numbers in tests
std::random_device rd;
std::mt19937 gen (rd ());
std::uniform_int_distribution<xcb_pixmap_t> m_rnd (0, MAX_NUM);
std::uniform_int_distribution<uint16_t> s_rnd (0, SMALL_MAX_NUM);

BOOST_AUTO_TEST_CASE (send_vecotrs)
{
  // Initialize test desktop structs
  dexpo_pixmap d0{ 0, s_rnd (gen), s_rnd (gen), m_rnd (gen), "HDMI-0" };
  dexpo_pixmap d1{ 1, s_rnd (gen), s_rnd (gen), m_rnd (gen), "VDI-1" };
  dexpo_pixmap d2{ 2, s_rnd (gen), s_rnd (gen), m_rnd (gen), "DP-0" };
  dexpo_pixmap d3{ 3, s_rnd (gen), s_rnd (gen), m_rnd (gen), "VGA-1" };

  std::vector<dexpo_pixmap> v = { d0, d1, d2, d3 };

  auto daemon = dexpo_socket ();
  std::thread daemon_thread (&dexpo_socket::send_pixmaps_on_event, &daemon,
                             std::ref (v)); // Passing by reference
  daemon_thread.detach ();
  BOOST_TEST_MESSAGE ("Started thread");

  auto client = dexpo_socket ();
  BOOST_TEST_MESSAGE ("Created client");

  auto v_recv = client.get_pixmaps ();
  BOOST_TEST_MESSAGE ("Got pixmaps");

  daemon.running = false; // FIXME Ugly stopping thread

  for (size_t i = 0; i < v.size (); i++)
    {
      BOOST_CHECK_EQUAL (v_recv[i], v[i]);
    }
}
