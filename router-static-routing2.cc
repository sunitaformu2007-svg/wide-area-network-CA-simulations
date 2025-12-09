/*
 * Triangular WAN Topology with Redundant Connectivity
 * Three sites: HQ, Branch Office, Data Center with full mesh connectivity
 * 
 * Network Topology:
 *
 *          Branch (n1)
 *           /        \
 *      Net1 /          \ Net2
 *  5Mbps,2ms/            \5Mbps,2ms
 *         /              \
 *    HQ (n0)------------- DC (n2)
 *          Net3: 5Mbps,2ms
 *
 * Networks:
 * - Network 1 (10.1.1.0/24): HQ ↔ Branch
 * - Network 2 (10.1.2.0/24): Branch ↔ DC  
 * - Network 3 (10.1.3.0/24): HQ ↔ DC (primary path)
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TriangularWANTopology");

// Callback functions for link failure simulation
void
DisableLink(Ptr<Node> node, uint32_t interface)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetDown(interface);
    std::cout << "\n*** [" << Simulator::Now().GetSeconds() << "s] LINK FAILURE ***\n";
    std::cout << "    Node " << node->GetId() << " interface " << interface << " DISABLED\n";
    std::cout << "    Traffic rerouting to backup path...\n\n";
}

void
EnableLink(Ptr<Node> node, uint32_t interface)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetUp(interface);
    std::cout << "\n*** [" << Simulator::Now().GetSeconds() << "s] LINK RECOVERY ***\n";
    std::cout << "    Node " << node->GetId() << " interface " << interface << " RE-ENABLED\n\n";
}

// Packet tracing callbacks
void
TxTrace(std::string context, Ptr<const Packet> packet)
{
    std::cout << "[" << Simulator::Now().GetSeconds() << "s] TX - " 
              << packet->GetSize() << " bytes\n";
}

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address& address)
{
    std::cout << "[" << Simulator::Now().GetSeconds() << "s] RX - " 
              << packet->GetSize() << " bytes\n";
}

int
main(int argc, char* argv[])
{
    // Command line parameters
    bool enableFailure = true;
    double failureTime = 5.0;
    double recoveryTime = 8.0;
    bool verbose = true;
    
    CommandLine cmd;
    cmd.AddValue("enableFailure", "Enable link failure simulation", enableFailure);
    cmd.AddValue("failureTime", "Time to simulate link failure (seconds)", failureTime);
    cmd.AddValue("recoveryTime", "Time to recover link (seconds)", recoveryTime);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);

    // Enable logging
    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║   Triangular WAN Topology Simulation                 ║\n";
    std::cout << "║   Multi-Site Network with Redundant Connectivity     ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    // ==================== TOPOLOGY CREATION ====================
    
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> n0 = nodes.Get(0); // HQ
    Ptr<Node> n1 = nodes.Get(1); // Branch
    Ptr<Node> n2 = nodes.Get(2); // DC

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link 1: HQ ↔ Branch (Network 1)
    NodeContainer link1Nodes(n0, n1);
    NetDeviceContainer link1Devices = p2p.Install(link1Nodes);

    // Link 2: Branch ↔ DC (Network 2)
    NodeContainer link2Nodes(n1, n2);
    NetDeviceContainer link2Devices = p2p.Install(link2Nodes);

    // Link 3: HQ ↔ DC (Network 3) - PRIMARY PATH
    NodeContainer link3Nodes(n0, n2);
    NetDeviceContainer link3Devices = p2p.Install(link3Nodes);

    // ==================== MOBILITY MODEL ====================
    
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set positions for triangular layout
    n0->GetObject<MobilityModel>()->SetPosition(Vector(10.0, 2.0, 0.0));  // HQ (bottom)
    n1->GetObject<MobilityModel>()->SetPosition(Vector(2.0, 15.0, 0.0));  // Branch (top-left)
    n2->GetObject<MobilityModel>()->SetPosition(Vector(18.0, 15.0, 0.0)); // DC (top-right)

    // ==================== INTERNET STACK ====================
    
    InternetStackHelper stack;
    stack.Install(nodes);

    // ==================== IP ADDRESS ASSIGNMENT ====================
    
    // Network 1: HQ-Branch (10.1.1.0/24)
    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(link1Devices);

    // Network 2: Branch-DC (10.1.2.0/24)
    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address2.Assign(link2Devices);

    // Network 3: HQ-DC (10.1.3.0/24)
    Ipv4AddressHelper address3;
    address3.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address3.Assign(link3Devices);

    // ==================== STATIC ROUTING CONFIGURATION ====================
    
    // Enable IP forwarding on all nodes
    n0->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    n1->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    n2->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));

    Ipv4StaticRoutingHelper staticRoutingHelper;

    // --- HQ (n0) Routing ---
    Ptr<Ipv4StaticRouting> routingN0 = 
        staticRoutingHelper.GetStaticRouting(n0->GetObject<Ipv4>());
    
    // Primary: Route to Branch-DC network via Branch
    routingN0->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"),
                                 Ipv4Address("10.1.1.2"), 1, 0);
    // Backup: Route to Branch-DC network via DC
    routingN0->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"),
                                 Ipv4Address("10.1.3.2"), 2, 10);

    // --- Branch (n1) Routing ---
    Ptr<Ipv4StaticRouting> routingN1 = 
        staticRoutingHelper.GetStaticRouting(n1->GetObject<Ipv4>());
    
    // Primary: Route to HQ-DC network via HQ
    routingN1->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"),
                                 Ipv4Address("10.1.1.1"), 1, 0);
    // Backup: Route to HQ-DC network via DC
    routingN1->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"),
                                 Ipv4Address("10.1.2.2"), 2, 10);

    // --- DC (n2) Routing ---
    Ptr<Ipv4StaticRouting> routingN2 = 
        staticRoutingHelper.GetStaticRouting(n2->GetObject<Ipv4>());
    
    // Primary: Route to HQ-Branch network via HQ
    routingN2->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"),
                                 Ipv4Address("10.1.3.1"), 2, 0);
    // Backup: Route to HQ-Branch network via Branch
    routingN2->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"),
                                 Ipv4Address("10.1.2.1"), 1, 10);

    // Print routing tables
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>("triangular-routing.routes", std::ios::out);
    staticRoutingHelper.PrintRoutingTableAllAt(Seconds(1.0), routingStream);

    // ==================== APPLICATIONS ====================
    
    uint16_t port = 9;
    
    // UDP Echo Server on DC
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // UDP Echo Client on HQ targeting DC
    UdpEchoClientHelper echoClient(interfaces3.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(12));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(n0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(14.0));

    // ==================== FLOW MONITOR ====================
    
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // ==================== LINK FAILURE SIMULATION ====================
    
    if (enableFailure)
    {
        std::cout << "Link failure scheduled at t=" << failureTime << "s\n";
        std::cout << "Link recovery scheduled at t=" << recoveryTime << "s\n\n";
        
        // Disable HQ-DC link (interface 2 on n0)
        Simulator::Schedule(Seconds(failureTime), &DisableLink, n0, 2);
        
        // Re-enable link for recovery testing
        Simulator::Schedule(Seconds(recoveryTime), &EnableLink, n0, 2);
    }

    // ==================== TRACING ====================
    
    // Packet-level tracing
    Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpEchoClient/Tx",
                    MakeCallback(&TxTrace));
    Config::Connect("/NodeList/2/ApplicationList/*/$ns3::UdpEchoServer/Rx",
                    MakeCallback(&RxTrace));

    // PCAP tracing
    p2p.EnablePcapAll("triangular-wan");

    // ==================== NETANIM CONFIGURATION ====================
    
    AnimationInterface anim("triangular-wan.xml");
    anim.UpdateNodeDescription(n0, "HQ\n10.1.1.1\n10.1.3.1");
    anim.UpdateNodeDescription(n1, "Branch\n10.1.1.2\n10.1.2.1");
    anim.UpdateNodeDescription(n2, "DC\n10.1.2.2\n10.1.3.2");
    
    anim.UpdateNodeColor(n0, 0, 255, 0);   // Green (HQ)
    anim.UpdateNodeColor(n1, 255, 255, 0); // Yellow (Branch)
    anim.UpdateNodeColor(n2, 0, 0, 255);   // Blue (DC)

    // ==================== NETWORK CONFIGURATION SUMMARY ====================
    
    std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║             Network Configuration                     ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════╣\n";
    std::cout << "║ HQ (n0)      Interface 1: " << interfaces1.GetAddress(0) << "        ║\n";
    std::cout << "║              Interface 2: " << interfaces3.GetAddress(0) << "        ║\n";
    std::cout << "╟───────────────────────────────────────────────────────╢\n";
    std::cout << "║ Branch (n1)  Interface 1: " << interfaces1.GetAddress(1) << "        ║\n";
    std::cout << "║              Interface 2: " << interfaces2.GetAddress(0) << "        ║\n";
    std::cout << "╟───────────────────────────────────────────────────────╢\n";
    std::cout << "║ DC (n2)      Interface 1: " << interfaces2.GetAddress(1) << "        ║\n";
    std::cout << "║              Interface 2: " << interfaces3.GetAddress(1) << "        ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    std::cout << "Primary Path: HQ → DC (direct, 4ms latency)\n";
    std::cout << "Backup Path:  HQ → Branch → DC (8ms latency)\n\n";

    // ==================== RUN SIMULATION ====================
    
    Simulator::Stop(Seconds(15.0));
    std::cout << "Starting simulation...\n\n";
    Simulator::Run();

    // ==================== FLOW STATISTICS ====================
    
    std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║           Flow Monitor Statistics                     ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = 
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        
        std::cout << "Flow ID: " << iter->first << "\n";
        std::cout << "  Src: " << t.sourceAddress << ":" << t.sourcePort << "\n";
        std::cout << "  Dst: " << t.destinationAddress << ":" << t.destinationPort << "\n";
        std::cout << "  ─────────────────────────────────────\n";
        std::cout << "  Tx Packets:  " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets:  " << iter->second.rxPackets << "\n";
        std::cout << "  Lost:        " << iter->second.lostPackets << "\n";
        
        if (iter->second.rxPackets > 0)
        {
            double avgDelay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;
            std::cout << "  Avg Delay:   " << avgDelay * 1000 << " ms\n";
            
            double throughput = iter->second.rxBytes * 8.0 / 
                               (iter->second.timeLastRxPacket.GetSeconds() - 
                                iter->second.timeFirstTxPacket.GetSeconds()) / 1024;
            std::cout << "  Throughput:  " << throughput << " Kbps\n";
            
            double pdr = (double)iter->second.rxPackets / 
                        (double)iter->second.txPackets * 100;
            std::cout << "  PDR:         " << pdr << "%\n";
        }
        std::cout << "\n";
    }

    // Save detailed statistics
    monitor->SerializeToXmlFile("triangular-flow-stats.xml", true, true);

    Simulator::Destroy();

    // ==================== SUMMARY ====================
    
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║           Simulation Complete                         ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════╣\n";
    std::cout << "║ Output Files:                                         ║\n";
    std::cout << "║  • triangular-wan.xml (NetAnim visualization)         ║\n";
    std::cout << "║  • triangular-routing.routes (routing tables)         ║\n";
    std::cout << "║  • triangular-wan-*.pcap (packet captures)            ║\n";
    std::cout << "║  • triangular-flow-stats.xml (flow statistics)        ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════╣\n";
    std::cout << "║ Analysis Tools:                                       ║\n";
    std::cout << "║  • NetAnim: Visualize topology and packet flow        ║\n";
    std::cout << "║  • Wireshark: Analyze PCAP traces                     ║\n";
    std::cout << "║  • FlowMonitor XML: Detailed performance metrics      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    return 0;
}
