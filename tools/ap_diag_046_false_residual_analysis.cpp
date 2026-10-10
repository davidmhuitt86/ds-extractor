#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;
namespace {
constexpr std::size_t kExpected=25U;
constexpr double kObjectPx=3.0;
constexpr double kAssocPx=6.0;
struct Residual { std::string endpoint_id,node_id,gt; double gt_distance=0.0; };
struct Object { bool matched=false; std::string id,kind; double distance=std::numeric_limits<double>::infinity(); };
struct Ink { double c5=0,l17=0,h=0,v=0; };
struct Record {
 Residual src; Point2D node_pos{},endpoint_pos{}; std::string node_type;
 std::size_t degree=0,unique_segments=0,repeated=0; double endpoint_node_distance=0;
 EndpointKind endpoint_kind=EndpointKind::GeometricConductorEnd;
 TerminalRole endpoint_role=TerminalRole::ExternalConnection;
 Object component,connector; Ink ink; std::string classification; std::vector<std::string> basis;
};
double dist(Point2D a,Point2D b){return std::hypot(a.x-b.x,a.y-b.y);}
std::string esc(const std::string& s){std::string o;for(char c:s){switch(c){case '\\':o+="\\\\\";break;
case '"':o+="\\\"";break;case '\n':o+="\\n";break;case '\r':o+="\\r";break;case '\t':o+="\\t";break;default:o+=c;}}return o;}
std::vector<Residual> load(const fs::path& p){
 std::ifstream in(p);if(!in)throw std::runtime_error("Unable to open AP-DIAG-044 report: "+p.string());
 std::ostringstream b;b<<in.rdbuf();const std::string t=b.str();
 const std::regex re(R"rx("endpoint_id"\s*:\s*"([^"]+)"[\s\S]*?"residual_node_id"\s*:\s*"([^"]+)"[\s\S]*?"nearest_ground_truth_splice"\s*:\s*"([^"]+)"[\s\S]*?"distance_px"\s*:\s*([0-9eE+.-]+)[\s\S]*?"matches_ground_truth"\s*:\s*false)rx");
 std::vector<Residual> v;for(std::sregex_iterator i(t.begin(),t.end(),re),e;i!=e;++i)v.push_back({(*i)[1].str(),(*i)[2].str(),(*i)[3].str(),std::stod((*i)[4].str())});
 if(v.size()!=kExpected)throw std::runtime_error("Expected exactly 25 false AP-DIAG-044 residuals; found "+std::to_string(v.size()));
 return v;
}
const TopologyNode* node(const WireModel&m,const std::string&id){auto i=std::find_if(m.nodes.begin(),m.nodes.end(),[&](const auto&n){return n.id==id;});return i==m.nodes.end()?nullptr:&*i;}
const EndpointCandidate* endpoint(const WireModel&m,const std::string&id){auto i=std::find_if(m.endpoint_candidates.begin(),m.endpoint_candidates.end(),[&](const auto&e){return e.id==id;});return i==m.endpoint_candidates.end()?nullptr:&*i;}
double boxdist(const BoundingBox&b,Point2D p){double l=b.x,t=b.y,r=l+b.width,bot=t+b.height;return std::hypot(std::max({l-p.x,0.0,p.x-r}),std::max({t-p.y,0.0,p.y-bot}));}
Object comp(const WireModel&m,Point2D p){Object o;for(const auto&c:m.component_candidates){if(c.kind==ComponentCandidateKind::DiagramFurniture)continue;double d=boxdist(c.bounds,p);if(d<o.distance){o.distance=d;o.id=c.id;switch(c.kind){case ComponentCandidateKind::Enclosure:o.kind="enclosure";break;case ComponentCandidateKind::CircularSymbol:o.kind="circular_symbol";break;case ComponentCandidateKind::ChassisGround:o.kind="chassis_ground";break;case ComponentCandidateKind::PrimitiveSymbol:o.kind="primitive_symbol";break;default:o.kind="unknown";}}}o.matched=o.distance<=kObjectPx;return o;}
Object conn(const WireModel&m,Point2D p){Object o;o.kind="connector";for(const auto&c:m.connector_candidates){double d=boxdist(c.bounds,p);if(d<o.distance){o.distance=d;o.id=c.id;}}o.matched=o.distance<=kObjectPx;return o;}
std::string nt(TopologyNodeType t){switch(t){case TopologyNodeType::ConductorEnd:return"conductor_end";case TopologyNodeType::Continuation:return"continuation";case TopologyNodeType::Junction:return"junction";case TopologyNodeType::Splice:return"splice";case TopologyNodeType::Crossing:return"crossing";case TopologyNodeType::ComponentBoundary:return"component_boundary";default:return"unresolved";}}
std::string ek(EndpointKind k){switch(k){case EndpointKind::GeometricConductorEnd:return"geometric_conductor_end";case EndpointKind::ComponentTerminal:return"component_terminal";case EndpointKind::ConnectorTerminal:return"connector_terminal";case EndpointKind::Splice:return"splice";case EndpointKind::Ground:return"ground";case EndpointKind::ExternalConnection:return"external_connection";default:return"unresolved";}}
std::string tr(TerminalRole r){switch(r){case TerminalRole::ComponentTerminal:return"component_terminal";case TerminalRole::ConnectorTerminal:return"connector_terminal";case TerminalRole::GroundTerminal:return"ground_terminal";case TerminalRole::PowerSource:return"power_source";case TerminalRole::ExternalConnection:return"external_connection";default:return"unknown";}}
double density(const cv::Mat&g,int x,int y,int rad){int l=std::max(0,x-rad),r=std::min(g.cols-1,x+rad),t=std::max(0,y-rad),b=std::min(g.rows-1,y+rad);std::size_t d=0,n=0;for(int yy=t;yy<=b;++yy)for(int xx=l;xx<=r;++xx){++n;if(g.at<unsigned char>(yy,xx)<150)++d;}return n?double(d)/double(n):0;}
double ray(const cv::Mat&g,int x,int y,int dx,int dy){std::size_t d=0,n=0;for(int z=5;z<=18;++z)for(int w=-1;w<=1;++w){int sx=x+dx*z,sy=y+dy*z;if(dx)sy+=w;else sx+=w;if(sx<0||sy<0||sx>=g.cols||sy>=g.rows)continue;++n;if(g.at<unsigned char>(sy,sx)<150)++d;}return n?double(d)/double(n):0;}
Ink ink(const cv::Mat&g,Point2D p){int x=int(std::lround(p.x)),y=int(std::lround(p.y));double l=ray(g,x,y,-1,0),r=ray(g,x,y,1,0),u=ray(g,x,y,0,-1),d=ray(g,x,y,0,1);return{density(g,x,y,2),density(g,x,y,8),(l+r)/2,(u+d)/2};}
std::string classify(const Record&r){
 if(r.connector.matched&&r.component.matched)return"AMBIGUOUS_OBJECT_CANDIDATE";
 if(r.connector.matched)return"CONNECTOR_BODY_CANDIDATE";
 if(r.component.matched)return"COMPONENT_BODY_CANDIDATE";
 if(r.node_type=="crossing")return"CROSSING_CANDIDATE";
 if(r.node_type=="conductor_end"&&r.degree<=1)return"CONDUCTOR_END_CANDIDATE";
 if(r.node_type=="splice"&&r.degree>=3)return"SPLICE_CANDIDATE";
 if(r.endpoint_node_distance>kAssocPx)return"ENDPOINT_NODE_SEPARATION_CANDIDATE";
 return"OTHER_TOPOLOGY_CANDIDATE";
}
void basis(Record&r){r.basis={"node_type_"+r.node_type,"topology_degree_"+std::to_string(r.degree),
 "endpoint_node_distance_"+(r.endpoint_node_distance>kAssocPx?"gt_6px":"le_6px")};
 if(r.connector.matched)r.basis.push_back("connector_bounds_within_3px");
 if(r.component.matched)r.basis.push_back("component_bounds_within_3px");
 if(r.node_type=="crossing")r.basis.push_back("production_node_is_crossing");
 if(r.node_type=="conductor_end"&&r.degree<=1)r.basis.push_back("degree_1_or_less");
 if(r.node_type=="splice"&&r.degree>=3)r.basis.push_back("production_node_is_splice_degree_3_plus");
 if(r.endpoint_kind==EndpointKind::GeometricConductorEnd)r.basis.push_back("residual_endpoint_geometric");
}
void crop(const cv::Mat&s,const Record&r,const fs::path&p){int x=int(std::lround(r.node_pos.x)),y=int(std::lround(r.node_pos.y)),pad=36,l=std::max(0,x-pad),t=std::max(0,y-pad),rr=std::min(s.cols,x+pad+1),b=std::min(s.rows,y+pad+1);cv::Mat c=s(cv::Rect(l,t,rr-l,b-t)).clone();cv::resize(c,c,cv::Size(),5,5,cv::INTER_NEAREST);cv::circle(c,{(x-l)*5,(y-t)*5},12,cv::Scalar(0,0,255),2,cv::LINE_AA);if(!cv::imwrite(p.string(),c))throw std::runtime_error("Unable to write crop: "+p.string());}
void sheet(const std::vector<fs::path>&ps,const fs::path&p){int W=260,H=260,C=5,R=int((ps.size()+C-1)/C);cv::Mat s(R*H,C*W,CV_8UC3,cv::Scalar(255,255,255));for(std::size_t i=0;i<ps.size();++i){cv::Mat im=cv::imread(ps[i].string(),cv::IMREAD_COLOR);if(im.empty())throw std::runtime_error("Unable to read crop: "+ps[i].string());cv::resize(im,im,cv::Size(W,H));im.copyTo(s(cv::Rect(int(i%C)*W,int(i/C)*H,W,H)));}if(!cv::imwrite(p.string(),s))throw std::runtime_error("Unable to write contact sheet: "+p.string());}
void report(const fs::path&p,const WireModel&m,const std::vector<Record>&rs){
 std::ofstream o(p);if(!o)throw std::runtime_error("Unable to write AP-DIAG-046 report: "+p.string());std::map<std::string,std::size_t> counts;for(const auto&r:rs)++counts[r.classification];
 o<<"{\n  \"schema_version\": 1,\n  \"ap\": \"AP-DIAG-046\",\n  \"status\": \"diagnostic_only\",\n  \"production_logic_modified\": false,\n  \"population\": {\"expected_false_residuals\": "<<rs.size()<<", \"source_model_wires\": "<<m.wires.size()<<", \"source_model_endpoints\": "<<m.endpoint_candidates.size()<<", \"source_model_nodes\": "<<m.nodes.size()<<", \"source_model_edges\": "<<m.edges.size()<<"},\n  \"classification_counts\": {";
 std::size_t z=0;for(const auto&[n,c]:counts)o<<"\n    \""<<esc(n)<<"\": "<<c<<(++z==counts.size()?"":",");o<<"\n  },\n  \"records\": [\n";
 for(std::size_t i=0;i<rs.size();++i){const auto&r=rs[i];o<<"    {\n      \"endpoint_id\": \""<<esc(r.src.endpoint_id)<<"\",\n      \"residual_node_id\": \""<<esc(r.src.node_id)<<"\",\n      \"nearest_ground_truth_splice\": \""<<esc(r.src.gt)<<"\",\n      \"ground_truth_distance_px\": "<<r.src.gt_distance<<",\n      \"node_position\": {\"x\": "<<r.node_pos.x<<", \"y\": "<<r.node_pos.y<<"},\n      \"endpoint_position\": {\"x\": "<<r.endpoint_pos.x<<", \"y\": "<<r.endpoint_pos.y<<"},\n      \"node_type\": \""<<r.node_type<<"\",\n      \"degree\": "<<r.degree<<",\n      \"unique_segments\": "<<r.unique_segments<<",\n      \"repeated_segment_incidents\": "<<r.repeated<<",\n      \"endpoint_node_distance_px\": "<<r.endpoint_node_distance<<",\n      \"endpoint_kind\": \""<<ek(r.endpoint_kind)<<"\",\n      \"terminal_role\": \""<<tr(r.endpoint_role)<<"\",\n      \"nearest_component\": {\"matched\": "<<(r.component.matched?"true":"false")<<", \"id\": \""<<esc(r.component.id)<<"\", \"kind\": \""<<esc(r.component.kind)<<"\", \"distance_px\": "<<r.component.distance<<"},\n      \"nearest_connector\": {\"matched\": "<<(r.connector.matched?"true":"false")<<", \"id\": \""<<esc(r.connector.id)<<"\", \"distance_px\": "<<r.connector.distance<<"},\n      \"ink\": {\"center_5x5\": "<<r.ink.c5<<", \"local_17x17\": "<<r.ink.l17<<", \"horizontal_support\": "<<r.ink.h<<", \"vertical_support\": "<<r.ink.v<<"},\n      \"classification\": \""<<r.classification<<"\",\n      \"basis\": [";
 for(std::size_t j=0;j<r.basis.size();++j){if(j)o<<", ";o<<"\""<<esc(r.basis[j])<<"\"";}o<<"]\n    }"<<(i+1==rs.size()?"":",")<<"\n";}
 o<<"  ]\n}\n";
}
int run(int argc,char**argv){
 if(argc<3){std::cerr<<"Usage: dx-audit-false-residual-analysis <image> <output_dir>\n";return 2;}
 fs::path image=argv[1],outdir=argv[2],ap044=outdir/"AP-DIAG-044_splice_ground_truth.json",report_path=outdir/"AP-DIAG-046_false_residual_analysis.json",cropdir=fs::path("artifacts")/"false_residual_analysis";fs::create_directories(outdir);fs::create_directories(cropdir);
 auto rs=load(ap044);cv::Mat src=cv::imread(image.string(),cv::IMREAD_COLOR);if(src.empty())throw std::runtime_error("Unable to load source image: "+image.string());cv::Mat gray;cv::cvtColor(src,gray,cv::COLOR_BGR2GRAY);
 ExtractionPipeline pipeline;WireModel m=pipeline.run(image.string(),image.string());if(m.image_width!=898||m.image_height!=549)throw std::runtime_error("Expected canonical 898x549 source dimensions.");if(m.wires.size()!=77||m.endpoint_candidates.size()!=189||m.nodes.size()!=532||m.edges.size()!=643)throw std::runtime_error("Current model population differs from AP-DIAG-044 baseline.");
 std::vector<Record> records;records.reserve(rs.size());std::vector<fs::path> crops;crops.reserve(rs.size());
 for(std::size_t i=0;i<rs.size();++i){const auto*n=node(m,rs[i].node_id);const auto*e=endpoint(m,rs[i].endpoint_id);if(!n)throw std::runtime_error("Residual node not found: "+rs[i].node_id);if(!e)throw std::runtime_error("Residual endpoint not found: "+rs[i].endpoint_id);Record r;r.src=rs[i];r.node_pos=n->position;r.endpoint_pos=e->position;r.node_type=nt(n->type);r.endpoint_kind=e->kind;r.endpoint_role=e->terminal_role;r.endpoint_node_distance=dist(n->position,e->position);r.component=comp(m,n->position);r.connector=conn(m,n->position);r.ink=ink(gray,n->position);std::set<std::string> segs;std::map<std::string,std::size_t> sc;for(const auto&ed:m.edges)if(ed.from_node==n->id||ed.to_node==n->id){++r.degree;if(!ed.conductor_segment.empty()){segs.insert(ed.conductor_segment);++sc[ed.conductor_segment];}}r.unique_segments=segs.size();for(const auto&[s,c]:sc){(void)s;if(c>1)r.repeated+=c;}r.classification=classify(r);basis(r);records.push_back(r);std::ostringstream name;name<<"residual-"<<std::setfill('0')<<std::setw(3)<<(i+1)<<".png";fs::path cp=cropdir/name.str();crop(src,r,cp);crops.push_back(cp);}
 sheet(crops,cropdir/"AP-DIAG-046_contact_sheet.png");report(report_path,m,records);std::map<std::string,std::size_t> counts;for(const auto&r:records)++counts[r.classification];
 std::cout<<"[AP-DIAG-046] False residuals            : "<<records.size()<<"\n"<<"[AP-DIAG-046] Current model wires         : "<<m.wires.size()<<"\n"<<"[AP-DIAG-046] Current model endpoints     : "<<m.endpoint_candidates.size()<<"\n"<<"[AP-DIAG-046] Current topology nodes      : "<<m.nodes.size()<<"\n";for(const auto&[n,c]:counts)std::cout<<"[AP-DIAG-046] "<<std::left<<std::setw(34)<<n<<": "<<c<<"\n";std::cout<<"[AP-DIAG-046] Report: "<<report_path.string()<<"\n[AP-DIAG-046] Contact sheet: "<<(cropdir/"AP-DIAG-046_contact_sheet.png").string()<<"\n[AP-DIAG-046] Diagnostic only; no production source modified.\n";return 0;
}
}
int main(int argc,char**argv){try{return run(argc,argv);}catch(const std::exception&e){std::cerr<<"[AP-DIAG-046] ERROR: "<<e.what()<<"\n";return 1;}}
