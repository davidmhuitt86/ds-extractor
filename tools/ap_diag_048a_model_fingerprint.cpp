#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
using namespace eke::dx::wire;
namespace {
std::string esc(const std::string& s){std::string o;for(char c:s){if(c=='\\'||c=='"')o+='\\';if(c=='\n'){o+="\\n";continue;}o+=c;}return o;}
std::string nt(TopologyNodeType t){switch(t){case TopologyNodeType::ConductorEnd:return "conductor_end";case TopologyNodeType::Continuation:return "continuation";case TopologyNodeType::Junction:return "junction";case TopologyNodeType::Splice:return "splice";case TopologyNodeType::Crossing:return "crossing";case TopologyNodeType::ComponentBoundary:return "component_boundary";default:return "unresolved";}}
std::string ek(EndpointKind k){switch(k){case EndpointKind::GeometricConductorEnd:return "geometric_conductor_end";case EndpointKind::ComponentTerminal:return "component_terminal";case EndpointKind::ConnectorTerminal:return "connector_terminal";case EndpointKind::Splice:return "splice";case EndpointKind::Ground:return "ground";case EndpointKind::ExternalConnection:return "external_connection";default:return "unresolved";}}
}
int main(int argc,char** argv){try{
 if(argc!=3){std::cerr<<"Usage: dx-audit-model-fingerprint <source-image> <output-json>\n";return 2;}
 fs::path source=argv[1],output=argv[2];cv::Mat im=cv::imread(source.string(),cv::IMREAD_GRAYSCALE);if(im.empty())throw std::runtime_error("Cannot load source image");
 ExtractionPipeline p;WireModel m=p.run(source.string(),source.filename().string());
 fs::create_directories(output.parent_path().empty()?fs::path("."):output.parent_path());
 std::ofstream o(output);if(!o)throw std::runtime_error("Cannot open output");
 o<<"{\n  \"ap\": \"AP-DIAG-048A\",\n  \"diagnostic_only\": true,\n  \"source\": \""<<esc(source.string())<<"\",\n  \"image\": {\"width\": "<<im.cols<<", \"height\": "<<im.rows<<"},\n";
 o<<"  \"counts\": {\"wires\": "<<m.wires.size()<<", \"endpoints\": "<<m.endpoint_candidates.size()<<", \"nodes\": "<<m.nodes.size()<<", \"edges\": "<<m.edges.size()<<", \"components\": "<<m.component_candidates.size()<<", \"connectors\": "<<m.connector_candidates.size()<<"},\n";
 o<<"  \"nodes\": [";bool first=true;for(const auto& n:m.nodes){if(!first)o<<",";first=false;o<<"\n    {\"id\":\""<<esc(n.id)<<"\",\"x\":"<<n.position.x<<",\"y\":"<<n.position.y<<",\"type\":\""<<nt(n.type)<<"\",\"electrically_connective\":"<<(n.electrically_connective?"true":"false")<<"}";}o<<"\n  ],\n";
 o<<"  \"endpoints\": [";first=true;for(const auto& e:m.endpoint_candidates){if(!first)o<<",";first=false;o<<"\n    {\"id\":\""<<esc(e.id)<<"\",\"node_id\":\""<<esc(e.node_id)<<"\",\"x\":"<<e.position.x<<",\"y\":"<<e.position.y<<",\"kind\":\""<<ek(e.kind)<<"\",\"terminal_role\":"<<static_cast<int>(e.terminal_role)<<",\"confidence\":"<<static_cast<int>(e.confidence)<<",\"incident_edges\":[";for(std::size_t i=0;i<e.incident_edges.size();++i){if(i)o<<",";o<<"\""<<esc(e.incident_edges[i])<<"\"";}o<<"]}";}o<<"\n  ],\n";
 o<<"  \"wires\": [";first=true;for(const auto& w:m.wires){if(!first)o<<",";first=false;o<<"\n    {\"id\":\""<<esc(w.id)<<"\",\"start\":\""<<esc(w.start_endpoint)<<"\",\"end\":\""<<esc(w.end_endpoint)<<"\",\"segments\":[";for(std::size_t i=0;i<w.conductor_segments.size();++i){if(i)o<<",";o<<"\""<<esc(w.conductor_segments[i])<<"\"";}o<<"]}";}o<<"\n  ]\n}\n";if(!o)throw std::runtime_error("Write failed");
 std::cout<<"[AP-DIAG-048A] Wires: "<<m.wires.size()<<"\n[AP-DIAG-048A] Endpoints: "<<m.endpoint_candidates.size()<<"\n[AP-DIAG-048A] Nodes: "<<m.nodes.size()<<"\n[AP-DIAG-048A] Edges: "<<m.edges.size()<<"\n[AP-DIAG-048A] Report: "<<output.string()<<"\n[AP-DIAG-048A] Diagnostic only; production source unchanged.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"[AP-DIAG-048A] ERROR: "<<e.what()<<"\n";return 1;}}
