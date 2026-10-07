#include "features/interaction/RestrictionRegion.h"
#include "features/interaction/MiningSession.h"
void check(bool,char const*);
void restrictionRegionTests() {
    using namespace lamium::interaction;
    using lamium::overlay::Cell;
    using lamium::overlay::Face;
    Cell anchor{-17,64,31};
    for (auto axis : {Axis::X,Axis::Y,Axis::Z}) {
        RestrictionRegion plane{RestrictionMode::Plane,anchor,axis}, line{RestrictionMode::Line,anchor,axis};
        check(plane.contains(anchor) && line.contains(anchor), "anchor is in every restriction");
        auto planeCells = plane.preview(2), lineCells = line.preview(2);
        check(planeCells.size() == 25 && lineCells.size() == 5, "plane and line preview dimensions");
        for (auto cell : planeCells) check(plane.contains(cell), "overlay never advertises disallowed plane cells");
        for (auto cell : lineCells) check(line.contains(cell), "overlay never advertises disallowed line cells");
        Cell along = anchor;
        if (axis == Axis::X) along.x += 100;
        else if (axis == Axis::Y) along.y += 100;
        else along.z += 100;
        check(!plane.contains(along) && line.contains(along), "line follows face normal and is not preview-radius limited");
    }
    RestrictionRegion column{RestrictionMode::Column,anchor,Axis::X}, layer{RestrictionMode::Layer,anchor,Axis::Z};
    check(column.effectiveAxis() == Axis::Y && layer.effectiveAxis() == Axis::Y,
          "vertical modes report the effective axis rather than the captured face");
    check(column.contains({-17,-100,31}) && !column.contains({-16,64,31}), "column follows world vertical independently of face");
    check(layer.contains({500,64,-500}) && !layer.contains({-17,65,31}), "layer fixes world Y independently of face");
    check(normalAxis(Face::West) == normalAxis(Face::East) && normalAxis(Face::Up) == Axis::Y
        && normalAxis(Face::North) == Axis::Z, "opposing faces select the same restriction axis");
    check(layer.preview(0) == std::set<Cell>{anchor}, "zero-radius preview is the anchor");
    bool rejected = false;
    try { (void)layer.preview(17); } catch (std::invalid_argument const&) { rejected = true; }
    check(rejected, "preview rejects excessive work rather than silently truncating");
    RestrictionRegion edge{RestrictionMode::Layer,{std::numeric_limits<int>::max()-1,0,0},Axis::Y};
    check(edge.preview(1).size() == 6, "preview avoids integer overflow and retains face headroom");

    RestrictionRegion band{RestrictionMode::HeightBand,{3,64,3},Axis::X,2};
    check(band.effectiveAxis() == Axis::Y, "height band reports Y");
    check(band.contains({900,64,-900}) && band.contains({0,65,0}) && !band.contains({0,66,0})
          && !band.contains({0,63,0}), "height band spans the feet row and the rows above it");
    band.height = 1;
    check(band.contains({0,64,0}) && !band.contains({0,65,0}), "height band of one is the feet row");
    band.height = 3;
    check(band.preview(1).size() == 18, "height band preview stays inside its rows");

    PressAnchor press;
    RestrictionRegion anchored{RestrictionMode::Layer,{10,70,10},Axis::Y};
    auto make = [&] { return anchored; };
    check(press.allows({10,70,10}, true, 1, make) && press.region() == anchored, "the first block of a press anchors");
    check(press.allows({40,70,-3}, true, 1, make) && !press.allows({10,71,10}, true, 1, make),
          "later blocks in the same press must be in the region");
    anchored.anchor = {10,71,10};
    check(press.allows({10,71,10}, true, 2, make) && press.region() == anchored, "a new press anchors afresh");
    press.follow(false, 2);
    check(!press.region(), "a release ends the region");
    check(press.allows({0,0,0}, false, 2, make) && !press.region(), "a call without a held press does not anchor");

    using namespace lamium::interaction::mining;
    Session session;
    check(session.next(Gate::Proceed, true) == Step::Vanilla, "proceed runs vanilla");
    check(session.next(Gate::Pause, false) == Step::Keep && !session.pending(), "a pause without progress only waits");
    check(session.next(Gate::Pause, true) == Step::StopAndKeep && session.pending(), "a pause aborts cracking");
    check(session.next(Gate::Pause, false) == Step::Keep && session.pending(), "the restart stays owed while paused");
    check(session.next(Gate::Proceed, false) == Step::Start && !session.pending(), "the next allowed call restarts once");
    check(session.next(Gate::Proceed, false) == Step::Vanilla, "after the restart vanilla continues");
    check(session.next(Gate::End, true) == Step::End, "an end refuses the call");
    check(session.next(Gate::Restart, false) == Step::Start, "a fetched tool restarts");
    session.next(Gate::Pause, true);
    session.started();
    check(session.next(Gate::Proceed, true) == Step::Vanilla, "a vanilla start settles an owed restart");
}
