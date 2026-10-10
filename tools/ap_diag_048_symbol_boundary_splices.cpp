#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {
double box_distance(const BoundingBox& b, Point2D p) {
    const double left=b.x, top=b.y, right=b.x+b.width, bottom=b.y+b.height;
    return std::hypot(std::max({left-p.x,0.0,p.x-right}),
                      std::max({top-p.y,0.0,p.y-bottom}));
}
double ink_density(const cv::Mat& gray, Point2D p, int radius) {
    const int cx=static_cast<int>(std::lround(p.x));
    const int cy=static_cast<int>(std::lround(p.y));
    std::size_t dark=0,total=0;
    for(int y=std::max(0,cy-radius);y<=std::min(gray.rows-1,cy+radius);++y)
        for(int x=std::max(0,cx-radius);x<=std::min(gray.cols-1,cx+radius);++x) {
            ++total; if(gray.at<unsigned char>(y,x)<150) ++dark;
        }
    return total ? static_cast<double>(dark)/static_cast<double>(total) : 0.0;
}
std::string esc(const std::string& s) {
    std::string out;
    for(char c:s) { if(c=='\\' || c=='"') out+='\\'; if(c=='\n') {out+="\\n";continue;} out+=c; }
    return out;
}
}

int main(int argc,char** argv) {
    try {
        if(argc!=3) {
            std::cerr << "Usage: dx-audit-symbol-boundary-splices <source-image> <output-json>\n";
            return 2;
        }
        const fs::path source=argv[1], output=argv[2];
        cv::Mat image=cv::imread(source.string(),cv::IMREAD_GRAYSCALE);
        if(image.empty()) throw std::runtime_error("Unable to load source image: "+source.string());
        ExtractionPipeline pipeline;
        const WireModel model=pipeline.run(source.string(),source.filename().string());
        std::ofstream out(output);
        if(!out) throw std::runtime_error("Unable to open output: "+output.string());
        out << "{\n  \"ap\": \"AP-DIAG-048\",\n"
            << "  \"diagnostic_only\": true,\n"
            << "  \"production_logic_modified\": false,\n"
            << "  \"source\": \"" << esc(source.string()) << "\",\n"
            << "  \"image\": {\"width\": " << image.cols << ", \"height\": " << image.rows << "},\n"
            << "  \"population\": {\"wires\": " << model.wires.size()
            << ", \"endpoints\": " << model.endpoint_candidates.size()
            << ", \"nodes\": " << model.nodes.size()
            << ", \"edges\": " << model.edges.size()
            << ", \"components\": " << model.component_candidates.size()
            << ", \"connectors\": " << model.connector_candidates.size() << "},\n"
            << "  \"thresholds\": {\"object_bounds_px\": 3.0, \"ink_radius_px\": 2},\n"
            << "  \"splice_nodes\": [\n";
        bool first=true;
        std::size_t splice_count=0,near_object=0;
        for(const auto& node:model.nodes) {
            if(node.type!=TopologyNodeType::Splice) continue;
            ++splice_count;
            double nearest_component=std::numeric_limits<double>::infinity();
            std::string component_id,component_kind;
            for(const auto& c:model.component_candidates) {
                if(c.kind==ComponentCandidateKind::DiagramFurniture) continue;
                const double d=box_distance(c.bounds,node.position);
                if(d<nearest_component) {nearest_component=d;component_id=c.id;
                    component_kind=std::to_string(static_cast<int>(c.kind));}
            }
            double nearest_connector=std::numeric_limits<double>::infinity();
            std::string connector_id;
            for(const auto& c:model.connector_candidates) {
                const double d=box_distance(c.bounds,node.position);
                if(d<nearest_connector) {nearest_connector=d;connector_id=c.id;}
            }
            const bool component_near=nearest_component<=3.0;
            const bool connector_near=nearest_connector<=3.0;
            if(component_near||connector_near) ++near_object;
            std::set<std::string> segments,adjacent;
            std::vector<std::string> edges;
            std::size_t degree=0;
            for(const auto& edge:model.edges) {
                if(edge.from_node==node.id || edge.to_node==node.id) {
                    ++degree; edges.push_back(edge.id);
                    adjacent.insert(edge.from_node==node.id?edge.to_node:edge.from_node);
                    if(!edge.conductor_segment_id.empty()) segments.insert(edge.conductor_segment_id);
                }
            }
            if(!first) out << ",\n"; first=false;
            out << "    {\"node_id\": \"" << esc(node.id) << "\", \"x\": " << node.position.x
                << ", \"y\": " << node.position.y << ", \"degree\": " << degree
                << ", \"unique_segments\": " << segments.size()
                << ", \"incident_edges\": " << edges.size()
                << ", \"near_component\": " << (component_near?"true":"false")
                << ", \"nearest_component_id\": \"" << esc(component_id)
                << "\", \"component_bounds_distance_px\": " << nearest_component
                << ", \"component_kind_numeric\": \"" << component_kind
                << "\", \"near_connector\": " << (connector_near?"true":"false")
                << ", \"nearest_connector_id\": \"" << esc(connector_id)
                << "\", \"connector_bounds_distance_px\": " << nearest_connector
                << ", \"local_ink_density\": " << ink_density(image,node.position,2)
                << ", \"incident_edge_ids\": [";
            for(std::size_t i=0;i<edges.size();++i) {if(i)out<<", ";out<<"\""<<esc(edges[i])<<"\"";}
            out << "], \"adjacent_node_ids\": [";
            std::size_t j=0; for(const auto& id:adjacent){if(j++)out<<", ";out<<"\""<<esc(id)<<"\"";}
            out << "]}";
        }
        out << "\n  ],\n  \"summary\": {\"splice_nodes\": " << splice_count
            << ", \"within_3px_of_component_or_connector\": " << near_object
            << "}\n}\n";
        if(!out) throw std::runtime_error("Failed while writing output JSON");
        std::cout << "[AP-DIAG-048] Splice nodes analyzed: " << splice_count << "\n"
                  << "[AP-DIAG-048] Within 3px of component/connector bounds: " << near_object << "\n"
                  << "[AP-DIAG-048] Report: " << output.string() << "\n"
                  << "[AP-DIAG-048] Diagnostic only; no production source modified.\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "[AP-DIAG-048] ERROR: " << e.what() << "\n";
        return 1;
    }
}
