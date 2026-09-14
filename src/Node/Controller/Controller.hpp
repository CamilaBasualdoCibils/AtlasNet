#pragma once


namespace AtlasNet
{
    class AtlasNetNode;
    class Controller
    {   
        AtlasNetNode& node;
        public:
        Controller(AtlasNetNode& _node): node(_node) {}
        void Start();
        void Tick();
        void Stop();
    };
};