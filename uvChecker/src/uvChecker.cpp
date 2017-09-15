#include "uvChecker.h"
#include "uvPoint.h"
#include <map>
#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <set>

UvChecker::UvChecker()
{
}

UvChecker::~UvChecker()
{
}

MSyntax UvChecker::newSyntax()
{
    MSyntax syntax;
    syntax.addArg(MSyntax::kString);
    syntax.addFlag("-v", "-verbose", MSyntax::kBoolean);
    syntax.addFlag("-c", "-check", MSyntax::kUnsigned);
    return syntax;
}

MStatus UvChecker::doIt(const MArgList& args)
{
    MStatus status;
    
    MSelectionList sel;

    MArgDatabase argData(syntax(), args);

    status = argData.getCommandArgument(0, sel);
    if (status != MS::kSuccess) {
        MGlobal::displayError("You have provide an object path");
        return MStatus::kFailure;
    }

    if (argData.isFlagSet("-verbose"))
        argData.getFlagArgument("-verbose", 0, verbose);
    else
        verbose = false;

    if (argData.isFlagSet("-check"))
        argData.getFlagArgument("-check", 0, checkNumber);
    else
        checkNumber = 99;

    sel.getDagPath(0, mDagPath);

    status = mDagPath.extendToShape();
    CHECK_MSTATUS_AND_RETURN_IT(status);

    if (mDagPath.apiType() != MFn::kMesh) {
        MGlobal::displayError("Selected object is not mesh.");
        return MStatus::kFailure;
    }

    return redoIt();
}

MStatus UvChecker::redoIt()
{
    MStatus status;

    switch (checkNumber) {
    case UvChecker::OVERLAPS:
        status = findOverlaps();
        MGlobal::displayInfo("Checking overlaps");
        CHECK_MSTATUS_AND_RETURN_IT(status);
        break;
    case UvChecker::UDIM:
        status = findUdimIntersections();
        MGlobal::displayInfo("Checking udim borders");
        CHECK_MSTATUS_AND_RETURN_IT(status);
        break;
    default:
        MGlobal::displayError("Invalid check number");
        break;
    }

    return MS::kSuccess;
}

MStatus UvChecker::undoIt()
{
    return MS::kSuccess;
}

bool UvChecker::isUndoable() const
{
    return false;
}

void* UvChecker::creater()
{
    return new UvChecker;
}

MStatus UvChecker::findOverlaps()
{
    MStatus status;

    MItMeshPolygon itPoly(mDagPath);
    MStringArray resultArray;

    int numTriangles;
    int numVertices;
    MIntArray vtxArray;
    std::map<int, int> vtxMap;

    for (; !itPoly.isDone(); itPoly.next()) {
        itPoly.numTriangles(numTriangles);
        itPoly.getVertices(vtxArray);

        numVertices = vtxArray.length();
        for (int i = 0; i < numVertices; i++) {
            vtxMap[vtxArray[i]] = i;
        }

        MPointArray pointArray;
        MIntArray intArray;

        for (int i = 0; i < numTriangles; i++) {
            // Each triangles
            UVPoint uvPointArray[3];

            itPoly.getTriangle(i, pointArray, intArray);
            float u;
            float v;
            int uvId;
            for (unsigned int n = 0; n < intArray.length(); n++) {
                int localIndex = vtxMap[intArray[n]];
                itPoly.getUVIndex(localIndex, uvId, u, v);
                UVPoint point(u, v);
                uvPointArray[n] = point;
            }

            float& Ax = uvPointArray[0].u;
            float& Ay = uvPointArray[0].v;
            float& Bx = uvPointArray[1].u;
            float& By = uvPointArray[1].v;
            float& Cx = uvPointArray[2].u;
            float& Cy = uvPointArray[2].v;

            float area = ((Ax * (By - Cy)) + (Bx * (Cy - Ay)) + (Cx * (Ay - By))) / 2;

            if (area < 0) {
                MString index;
                index.set(itPoly.index());
                MString s = mDagPath.fullPathName() + ".f[" + index + "]";
                resultArray.append(s);
            }
        }
        vtxMap.clear();
    }
    MPxCommand::setResult(resultArray);

    return MS::kSuccess;
}

MStatus UvChecker::findUdimIntersections()
{
    MStatus status;

    MIntArray indexArray;
    MFnMesh fnMesh(mDagPath);

    std::set<int> indexSet;

    for (MItMeshPolygon mItPoly(mDagPath); !mItPoly.isDone(); mItPoly.next()) {

        int vCount = mItPoly.polygonVertexCount();
        int currentIndex;
        int nextIndex;
        float u1, v1, u2, v2;

        for (int i = 0; i < vCount; i++) {
            mItPoly.getUVIndex(i, currentIndex);

            if (i == vCount - 1) {
                mItPoly.getUVIndex(0, nextIndex);
            } else {
                mItPoly.getUVIndex(i + 1, nextIndex);
            }

            fnMesh.getUV(currentIndex, u1, v1);
            fnMesh.getUV(nextIndex, u2, v2);

            if (floor(u1) == floor(u2) && floor(v1) == floor(v2)) {
            } else {
                indexSet.insert(currentIndex);
                indexSet.insert(nextIndex);
            }
        }
    }

    std::set<int>::iterator indexSetIter;
    for (indexSetIter = indexSet.begin(); indexSetIter != indexSet.end(); ++indexSetIter) {
        indexArray.append(*indexSetIter);
    }

    unsigned int arrayLength = indexArray.length();
    MStringArray resultArray;
    for (unsigned int i = 0; i < arrayLength; i++) {
        MString index;
        index.set(indexArray[i]);
        MString s = mDagPath.fullPathName() + ".f[" + index + "]";
        resultArray.append(s);
    }
    MPxCommand::setResult(resultArray);

    return MS::kSuccess;
}
