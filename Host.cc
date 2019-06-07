//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#include <algorithm>

#include "Host.h"
#include <string.h>
using namespace std;
namespace aloha {
#define INFINITY (1e999)
Define_Module(Host);

Host::Host()
{
    endTxEvent = nullptr;
}

Host::~Host()
{
    delete lastPacket;
    cancelAndDelete(endTxEvent);
}

void Host::initialize()
{
    stateSignal = registerSignal("state");
    server = getModuleByPath("server");
    if (!server)
        throw cRuntimeError("server not found");

    int numHosts = getParentModule()->par("numHosts");
    hosts = (cModule**)malloc(numHosts * sizeof(cModule));
    distHosts = new double[numHosts];
    neighborSet = new bool[numHosts];
    childrens = new bool[numHosts]();
    energyHosts = new double[numHosts];
    shortestPath = new double[numHosts];
    throughPath = new int[numHosts];
    shortestPathDistance = INFINITY;

    for (int i = 0; i < numHosts; ++i)
    {
        char text[10] = {0};
        shortestPath[i] = INFINITY;


        sprintf(text, "host[%d]", i);
        hosts[i] = getModuleByPath(text);
        if (hosts[i]->getId() == getId())
        {
            hostId = i;
        }
    }

    txRate = par("txRate");
    iaTime = &par("iaTime");
    pkLenBits = &par("pkLenBits");

    slotTime = par("slotTime");
    isSlotted = slotTime > 0;
    WATCH(slotTime);
    WATCH(isSlotted);
    int *p2 = new int[10]();    // block of ten ints value initialized to 0

    endTxEvent = new cMessage("send/endTx");
    state = IDLE;
    emit(stateSignal, state);
    pkCounter = 0;
    WATCH((int&)state);
    WATCH(pkCounter);

    x = par("x").doubleValue();
    y = par("y").doubleValue();

    double serverX = server->par("x").doubleValue();
    double serverY = server->par("y").doubleValue();

    idleAnimationSpeed = par("idleAnimationSpeed");
    transmissionEdgeAnimationSpeed = par("transmissionEdgeAnimationSpeed");
    midtransmissionAnimationSpeed = par("midTransmissionAnimationSpeed");

    double dist = std::sqrt((x-serverX) * (x-serverX) + (y-serverY) * (y-serverY));
    radioDelay = dist / propagationSpeed;

    getDisplayString().setTagArg("p", 0, x);
    getDisplayString().setTagArg("p", 1, y);
    if (true || getId() == 3)
    {
        cMessage *msg = new cMessage("initTx");
        scheduleAt(simTime(), msg);
    }

    if (getId() == hosts[0]->getId())
    {
        {
            cMessage *msg = new cMessage("initBellmanFord");
            scheduleAt(simTime() + 0.2, msg);
        }
        {
            cMessage *msg = new cMessage("initDetection");
            scheduleAt(4, msg);
        }
    }

    cMessage *msg = new cMessage("initFamily");
    scheduleAt(3, msg);


    //scheduleAt(getNextTransmissionTime(), endTxEvent);
}

void Host::handleMessage(cMessage *msg)
{
    //ASSERT(msg == endTxEvent);
    cout << "I'm host[" << hostId << "] " << msg->getName() << endl;
    if (msg->isSelfMessage())
    {
        if (strcmp(msg->getName(), "initTx") == 0)
        {

            int numHosts = getParentModule()->par("numHosts");
            double maxRange = getParentModule()->par("maxRange");
            simtime_t _duration;
            for (int i = 0; i < numHosts; ++i)
            {
                if (getId() == hosts[i]->getId())
                    continue;
                // generate packet and schedule timer when it ends
                double hostX = hosts[i]->par("x").doubleValue();
                double hostY = hosts[i]->par("y").doubleValue();
                double dist = std::sqrt((x-hostX) * (x-hostX) + (y-hostY) * (y-hostY));
                distHosts[i] = dist;
                if (dist > maxRange)
                {
                    distHosts[i] = INFINITY;
                    continue;
                }

                radioDelay = dist / propagationSpeed;

                char pkname[40];
                sprintf(pkname, "locationPacket-%03d-#%03d", hostId, pkCounter++);

                EV << "generating packet " << pkname << endl;

                state = TRANSMIT;
                emit(stateSignal, state);

                cPacket *pk = new cPacket(pkname);
                char parPower[40] = {0};


                pk->setBitLength(pkLenBits->intValue());
                simtime_t duration = pk->getBitLength() / txRate;
                _duration = duration;
                pk->setKind(2);
                //pk->setName("loc");



                sendDirect(pk, radioDelay, duration, hosts[i]->gate("in"));








                // let visualization code know about the new packet
                if (transmissionRing != nullptr) {
                    delete lastPacket;
                    lastPacket = pk->dup();
                }
            }
        }
        else if (strcmp(msg->getName(), "initBellmanFord") == 0)
        {
            int hostNumber = getParentModule()->par("baseStationId");
            EV << "Running Bellman-Ford from base station node #" << hostNumber << endl;
            int numHosts = getParentModule()->par("numHosts");
            simtime_t _duration;
            shortestPath[hostId] = 0;
            shortestPathDistance = 0;
            for (int i = 0; i < numHosts; ++i)
            {
                 if (!neighborSet[i])
                     continue;
                 // generate packet and schedule timer when it ends
                 double hostX = hosts[i]->par("x").doubleValue();
                 double hostY = hosts[i]->par("y").doubleValue();
                 double dist = std::sqrt((x-hostX) * (x-hostX) + (y-hostY) * (y-hostY));
                 distHosts[i] = dist;
                 radioDelay = dist / propagationSpeed;

                 char pkname[60];
                 sprintf(pkname, "BellmanFord-%03d-d=%012.3f", 0, 0);

                 EV << "generating packet " << pkname << endl;

                 state = TRANSMIT;
                 emit(stateSignal, state);

                 cPacket *pk = new cPacket(pkname);



                 pk->setBitLength(pkLenBits->intValue());
                 simtime_t duration = pk->getBitLength() / txRate;
                 _duration = duration;
                 pk->setKind(3);


                 sendDirect(pk, radioDelay, duration, hosts[i]->gate("in"));

                if (transmissionRing != nullptr)
                {
                    delete lastPacket;
                    lastPacket = pk->dup();
                }
            }
        }
        else if (strcmp(msg->getName(), "initFamily") == 0)
        {


            int numHosts = getParentModule()->par("numHosts");
            simtime_t _duration;
            int i = -1;
            double min = INFINITY;
            for (int j = 0; j < numHosts; ++j)
            {
                if (shortestPath[j] == shortestPathDistance)
                {
                    //min = shortestPath[j];
                    i = j;
                }
            }
            //found my papa, now set myParent
            myParent = i;
            cout << "Distance: " << min << endl;
            cout << "Telling parent I'm its children #" << i << endl;
            // generate packet and schedule timer when it ends
            double hostX = hosts[i]->par("x").doubleValue();
            double hostY = hosts[i]->par("y").doubleValue();
            double dist = std::sqrt((x-hostX) * (x-hostX) + (y-hostY) * (y-hostY));
            distHosts[i] = dist;
            radioDelay = dist / propagationSpeed;

            char pkname[60];
            sprintf(pkname, "papa-%03d", hostId);

            EV << "generating packet " << pkname << endl;

            state = TRANSMIT;
            emit(stateSignal, state);

            cPacket *pk = new cPacket(pkname);



            pk->setBitLength(pkLenBits->intValue());
            simtime_t duration = pk->getBitLength() / txRate;
            _duration = duration;
            pk->setKind(4);


            sendDirect(pk, radioDelay, duration, hosts[i]->gate("in"));

            if (transmissionRing != nullptr)
            {
                delete lastPacket;
                lastPacket = pk->dup();
            }

        }
        else if (strcmp(msg->getName(), "initDetection") == 0)
        {
            int numHosts = getParentModule()->par("numHosts");
            for (int i = 0; i < numHosts; ++i)
            {
                 if (!childrens[i])
                     continue;
                 if (i == hostId)
                 {
                     continue;
                 }
                 sendDCT(i, 0, 0);
            }
        }
    }
    else    //message from the outside
    {
        //if msg is loc
        if(msg->getKind()==2)
        {


            cPacket *pkt = check_and_cast<cPacket *>(msg);
            EV << pkt->getName() << endl;
            int i=0;

            int numHosts = getParentModule()->par("numHosts");
            //"get sender x y"
            for (int i = 0; i < numHosts; ++i)
            {
                double hostX = hosts[i]->par("x").doubleValue();
                double hostY = hosts[i]->par("y").doubleValue();
                double maxRange = getParentModule()->par("maxRange");
                if (distHosts[i] <= maxRange and getId() != hosts[i]->getId())
                {
                    neighborSet[i]=true;
                    energyHosts[i]= calculateEnergeyConsumptionPerBit(hostX,hostY,0,0,pkt->getBitLength());
                    //energyHosts[i] = 1;
                }
                else
                {
                    neighborSet[i]=false;
                    energyHosts[i]=INFINITY;
                }
                for (int i = 0; i < numHosts; ++i)
                {
                    EV<<"Host "<<i<<": distance:"<< distHosts[i]<< "   energy:"<< energyHosts[i]<<"   neighbor?  "<<neighborSet[i]<<"   "<<endl;
                }

            }
        }
        else if (msg->getKind() == 3)
        {
            double newDistance = 0;
            int senderHost = 0;
            const char *messageText = msg->getName();
            sscanf(messageText, "%*18s%lf", &newDistance);
            sscanf(messageText, "%*12s%03d", &senderHost);
            int hostNumber = getParentModule()->par("baseStationId");
            EV << "Running Bellman-Ford from base station node #" << hostNumber << endl;
            int numHosts = getParentModule()->par("numHosts");
            simtime_t _duration;
            double _dist = distHosts[senderHost];
            EV << "My Distance to #" <<  senderHost << " d=" << _dist << endl;
            EV << "New Distance from #" <<  senderHost << " d=" << newDistance << endl;
            EV << "My Best d=" << shortestPathDistance << endl;
            if (std::pow(_dist,2) + newDistance < shortestPathDistance)
            {

                shortestPathDistance = std::pow(_dist,2) + newDistance;
                //TODO
                //fix the distance, should be powered by 2
                shortestPath[senderHost] = shortestPathDistance;
                EV << "Found better path through host[" << senderHost  << "] d=" << shortestPathDistance << endl;

            }
            else
            {
                return;
            }

            for (int i = 0; i < numHosts; ++i)
            {
                 if (!neighborSet[i])
                     continue;

                 double dist = distHosts[i];
                 radioDelay = dist / propagationSpeed;

                 char pkname[60];
                 sprintf(pkname, "BellmanFord-%03d-d=%012.5f", hostId, shortestPathDistance);

                 EV << "generating packet " << pkname << endl;

                 state = TRANSMIT;
                 emit(stateSignal, state);

                 cPacket *pk = new cPacket(pkname);



                 pk->setBitLength(pkLenBits->intValue());
                 simtime_t duration = pk->getBitLength() / txRate;
                 _duration = duration;
                 pk->setKind(3);


                 sendDirect(pk, radioDelay, duration, hosts[i]->gate("in"));

                if (transmissionRing != nullptr)
                {
                    delete lastPacket;
                    lastPacket = pk->dup();
                }
            }
        }
        else if (msg->getKind() == 4)
        {
            const char *messageText = msg->getName();
            int hostNumber = -1;
            //papa-000
            sscanf(messageText, "%*5s%3d", &hostNumber);
            if (hostNumber != -1)
            {
                childrens[hostNumber] = true;
            }

        }
        else if (msg->getKind() == 5)
        {
            recvDCT(msg);
        }
        else if (msg->getKind() == 6)
        {
            recvPTS(msg);
        }

    }
}


simtime_t Host::getNextTransmissionTime()
{
    simtime_t t = simTime() + iaTime->doubleValue();

    if (!isSlotted)
        return t;
    else
        // align time of next transmission to a slot boundary
        return slotTime * ceil(t/slotTime);
}

void Host::refreshDisplay() const
{
    cCanvas *canvas = getParentModule()->getCanvas();
    const int numCircles = 20;
    const double circleLineWidth = 10;

    // create figures on our first invocation
    if (!transmissionRing) {
        auto color = cFigure::GOOD_DARK_COLORS[getId() % cFigure::NUM_GOOD_DARK_COLORS];

        transmissionRing = new cRingFigure(("Host" + std::to_string(getIndex()) + "Ring").c_str());
        transmissionRing->setOutlined(false);
        transmissionRing->setFillColor(color);
        transmissionRing->setFillOpacity(0.25);
        transmissionRing->setFilled(true);
        transmissionRing->setVisible(false);
        transmissionRing->setZIndex(-1);
        canvas->addFigure(transmissionRing);

        for (int i = 0; i < numCircles; ++i) {
            auto circle = new cOvalFigure(("Host" + std::to_string(getIndex()) + "Circle" + std::to_string(i)).c_str());
            circle->setFilled(false);
            circle->setLineColor(color);
            circle->setLineOpacity(0.75);
            circle->setLineWidth(circleLineWidth);
            circle->setZoomLineWidth(true);
            circle->setVisible(false);
            circle->setZIndex(-0.5);
            transmissionCircles.push_back(circle);
            canvas->addFigure(circle);
        }
    }

    if (lastPacket) {
        // update transmission ring and circles
        if (transmissionRing->getAssociatedObject() != lastPacket) {
            transmissionRing->setAssociatedObject(lastPacket);
            for (auto c : transmissionCircles)
                c->setAssociatedObject(lastPacket);
        }

        simtime_t now = simTime();
        simtime_t frontTravelTime = now - lastPacket->getSendingTime();
        simtime_t backTravelTime = now - (lastPacket->getSendingTime() + lastPacket->getDuration());

        // conversion from time to distance in m using speed
        double frontRadius = std::min(ringMaxRadius, frontTravelTime.dbl() * propagationSpeed);
        double backRadius = backTravelTime.dbl() * propagationSpeed;
        double circleRadiusIncrement = circlesMaxRadius / numCircles;

        // update transmission ring geometry and visibility/opacity
        double opacity = 1.0;
        if (backRadius > ringMaxRadius) {
            transmissionRing->setVisible(false);
            transmissionRing->setAssociatedObject(nullptr);
        }
        else {
            transmissionRing->setVisible(true);
            transmissionRing->setBounds(cFigure::Rectangle(x - frontRadius, y - frontRadius, 2*frontRadius, 2*frontRadius));
            transmissionRing->setInnerRadius(std::max(0.0, std::min(ringMaxRadius, backRadius)));
            if (backRadius > 0)
                opacity = std::max(0.0, 1.0 - backRadius / circlesMaxRadius);
        }

        transmissionRing->setLineOpacity(opacity);
        transmissionRing->setFillOpacity(opacity/5);

        // update transmission circles geometry and visibility/opacity
        double radius0 = std::fmod(frontTravelTime.dbl() * propagationSpeed, circleRadiusIncrement);
        for (int i = 0; i < (int)transmissionCircles.size(); ++i) {
            double circleRadius = std::min(ringMaxRadius, radius0 + i * circleRadiusIncrement);
            if (circleRadius < frontRadius - circleRadiusIncrement/2 && circleRadius > backRadius + circleLineWidth/2) {
                transmissionCircles[i]->setVisible(true);
                transmissionCircles[i]->setBounds(cFigure::Rectangle(x - circleRadius, y - circleRadius, 2*circleRadius, 2*circleRadius));
                transmissionCircles[i]->setLineOpacity(std::max(0.0, 0.2 - 0.2 * (circleRadius / circlesMaxRadius)));
            }
            else
                transmissionCircles[i]->setVisible(false);
        }

        // compute animation speed
        double animSpeed = idleAnimationSpeed;
        if ((frontRadius >= 0 && frontRadius < circlesMaxRadius) || (backRadius >= 0 && backRadius < circlesMaxRadius))
            animSpeed = transmissionEdgeAnimationSpeed;
        if (frontRadius > circlesMaxRadius && backRadius < 0)
            animSpeed = midtransmissionAnimationSpeed;
        canvas->setAnimationSpeed(animSpeed, this);
    }
    else {
        // hide transmission rings, update animation speed
        if (transmissionRing->getAssociatedObject() != nullptr) {
            transmissionRing->setVisible(false);
            transmissionRing->setAssociatedObject(nullptr);

            for (auto c : transmissionCircles) {
                c->setVisible(false);
                c->setAssociatedObject(nullptr);
            }
            canvas->setAnimationSpeed(idleAnimationSpeed, this);
        }
    }

    // update host appearance (color and text)
    getDisplayString().setTagArg("t", 2, "#808000");
    if (state == IDLE) {
        getDisplayString().setTagArg("i", 1, "");
        getDisplayString().setTagArg("t", 0, "");
    }
    else if (state == TRANSMIT) {
        getDisplayString().setTagArg("i", 1, "yellow");
        getDisplayString().setTagArg("t", 0, "TRANSMIT");
    }
}
double Host::calculateEnergeyConsumptionPerBit(double x, double y,int numTx, int numRx ,int bitsCount)
{
    //TODO
    /*
    double energyPerBit = 0;
    double constSize = getParentModule()->par("constellation").doubleValue();
    double epsilon = 3*(std::sqrt(std::pow(2,constSize))-1)/(std::sqrt(std::pow(2,constSize))+1);
    double pBitError = getParentModule()->par("bitErrorProbability").doubleValue();

    double spectralDensity = getParentModule()->par("noiseSpectralDensity").doubleValue();
    double alpha = (epsilon / 0.35) - 1;

    energyPerBit = (2.0/3.0)*(1 + alpha) * std::pow(pBitError / 4 , -(1/(numTx*numRx)))*((std::pow(2,constSize) - 1)/(std::pow(constSize, (1/(numTx*numRx+1)))))*spectralDensity;
    double dSum = 0;
    for (int i = 1;i < numTx; ++i)
    {
        for (int j = 1;j < numRx; ++j)
        {
            //dSum += (4*PI * )
        }
    }
    */
    return (((this->x-x) * (this->x-x) + (this->y-y) * (this->y-y)));
}
void Host::recvPTS(cMessage* msg)
{
    const char *messageText = msg->getName();
    int numHosts = getParentModule()->par("numHosts");
    //PartnerSelect-000-pts(000,000)
    int senderHost = 0;
    sscanf(messageText, "%*14s%03d", &senderHost);
    for (int i = 0; i < numHosts; ++i)
    {
        double weight = 0;
        if (hostId == 6)
        {
            cout << "host["<< i <<  "] is children: " << !childrens[i] << endl;
            cout << "I am: " << hostId << endl;
        }
        if (!childrens[i]) //if not my child, skip
        {
            continue;
        }
        //else
        if (i == hostId)
        {
            continue;
        }
        sendDCT(i, 1, senderHost);
    }
    setPartner(senderHost);
    //Finished = true TODO
}
void Host::recvDCT(cMessage* msg)
{

    const char *messageText = msg->getName();
    int numHosts = getParentModule()->par("numHosts");
    //Detection-%03d-dct(%01d,%03d)
    int senderHost = 0;
    int isPaired = 0;
    int pairedId = 0;
    sscanf(messageText, "%*10s%03d", &senderHost);
    sscanf(messageText, "%*18s%1d", &isPaired);
    sscanf(messageText, "%*20s%03d", &pairedId);
    double gamma = getParentModule()->par("gamma");
    double maximalWeight = -INFINITY;
    int maximallWeightId = -1;
    if (isPaired == 0) //Case (a) papa has a no pair
    {

        for (int i = 0; i < numHosts; ++i)
        {
            double weight = 0;
            if (!childrens[i]) //if not my child, skip
            {
                continue;
            }
            //else
            //calculate the weight W_(u,w) eq. 7 page 6.
            //W_(u,w) = W_(child, self)

            weight = (0.5 - gamma) * getEnergy(i, hostId) + getEnergy(hostId, senderHost)/*TODO -otherThing() */;
            if (weight > maximalWeight)
            {
                maximalWeight = weight;
                maximallWeightId = i;
            }
        }
    }
    else // that is got dct(1,u) a paired message
    {
        for (int i = 0; i < numHosts; ++i)
        {
            double weight = 0;
            if (!childrens[i]) //if not my child, skip
            {
                continue;
            }
            //else
            //calculate the weight W_(u,w) eq. 7 page 6.
            //W_(u,w) = W_(child, self)

            weight = (0.5 - gamma) * getEnergy(i, hostId) + getEnergy(hostId, senderHost) /*TODO -otherMIMOs() */;
            if (weight > maximalWeight)
            {
                maximalWeight = weight;
                maximallWeightId = i;
            }
        }
    }

    if (maximalWeight > 0)
    {
        int w = maximallWeightId;   // that is the best node to paired with
        int u = hostId;             // that is me, self.
        if (w != hostId)
        {
            sendPTS(w);
            setPartner(w);
        }
        // for-loop send other children dct(1,w), that is I've a partner that is w
        for (int i = 0; i < numHosts; ++i)
        {

            if (!childrens[i] or i == w) //if not my child, skip
            {
                continue;
            }
            if (w == hostId)
            {
                continue;
            }
            //else
            sendDCT(i, 1, w);

        }
    }
    else //no one of children is a good pair, that is not improving the energy costs
    {
        // for-loop send children dct(0.0), that is I don't have a partner
        for (int i = 0; i < numHosts; ++i)
        {

            if (!childrens[i]) //if not my child, skip
            {
                continue;
            }
            if (i == hostId)
            {
                continue;
            }
            //else
            sendDCT(i, 0, 0);

        }
    }

}
void    Host::sendPTS(int targetHost)
{
    double maxRange = getParentModule()->par("maxRange");
    simtime_t _duration;

    // generate packet and schedule timer when it ends
    double hostX = hosts[targetHost]->par("x").doubleValue();
    double hostY = hosts[targetHost]->par("y").doubleValue();
    double dist = std::sqrt((x-hostX) * (x-hostX) + (y-hostY) * (y-hostY));


    radioDelay = dist / propagationSpeed;

    char pkname[40];
    sprintf(pkname, "PartnerSelect-%03d-pts(%03d,%03d)", hostId, targetHost, hostId);

    EV << "generating packet " << pkname << endl;

    state = TRANSMIT;
    emit(stateSignal, state);

    cPacket *pk = new cPacket(pkname);
    char parPower[40] = {0};


    pk->setBitLength(pkLenBits->intValue());
    simtime_t duration = pk->getBitLength() / txRate;
    pk->setKind(6);

    sendDirect(pk, radioDelay, duration, hosts[targetHost]->gate("in"));


    // let visualization code know about the new packet
    if (transmissionRing != nullptr)
    {
        delete lastPacket;
        lastPacket = pk->dup();
    }
}
void    Host::setPartner(int targetHost)
{
    myPartnerId = targetHost;
}
double  Host::getEnergy(int v, int u)
{
    Host *host_v = check_and_cast<Host *>(hosts[v]);
    return std::pow(host_v->distHosts[u],2);
}
void Host::sendDCT(int targetHost, bool paired, int hostId)
{
    double maxRange = getParentModule()->par("maxRange");
    simtime_t _duration;

    // generate packet and schedule timer when it ends
    double hostX = hosts[targetHost]->par("x").doubleValue();
    double hostY = hosts[targetHost]->par("y").doubleValue();
    double dist = std::sqrt((x-hostX) * (x-hostX) + (y-hostY) * (y-hostY));


    radioDelay = dist / propagationSpeed;

    char pkname[40];
    sprintf(pkname, "Detection-%03d-dct(%01d,%03d)", this->hostId, paired, hostId);

    EV << "generating packet " << pkname << endl;

    state = TRANSMIT;
    emit(stateSignal, state);

    cPacket *pk = new cPacket(pkname);
    char parPower[40] = {0};


    pk->setBitLength(pkLenBits->intValue());
    simtime_t duration = pk->getBitLength() / txRate;
    pk->setKind(5);

    sendDirect(pk, radioDelay, duration, hosts[targetHost]->gate("in"));


    // let visualization code know about the new packet
    if (transmissionRing != nullptr)
    {
        delete lastPacket;
        lastPacket = pk->dup();
    }
}

}; //namespace
