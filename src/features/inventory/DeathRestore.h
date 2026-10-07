#pragma once
// L-109: after picking up items following a death, put them back where they
// were at death (DeathLayout.h). Runs on client ticks; default off.
namespace lamium::inventory::death {
void start();
void stop();
}
