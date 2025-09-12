#include "pch.hpp"
#include "KDNet/KDNetInterface.hpp"
#include "Singleton.hpp"
class KDNetClient: public KDNetInterface, public Singleton<KDNetClient>
{

};