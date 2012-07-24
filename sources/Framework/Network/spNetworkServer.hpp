/*
 * Network server header
 * 
 * This file is part of the "SoftPixel Engine" (Copyright (c) 2008 by Lukas Hermanns)
 * See "SoftPixelEngine.hpp" for license information.
 */

#ifndef __SP_NETWORK_SERVER_H__
#define __SP_NETWORK_SERVER_H__


#include "Base/spStandard.hpp"

#ifdef SP_COMPILE_WITH_NETWORKSYSTEM


#include "Framework/Network/spNetworkMember.hpp"


namespace sp
{
namespace network
{


class NetworkServer : public NetworkMember
{
    
    public:
        
        NetworkServer(const NetworkAddress &Address);
        ~NetworkServer();
        
};


} // /namespace network

} // /namespace sp


#endif

#endif



// ================================================================================