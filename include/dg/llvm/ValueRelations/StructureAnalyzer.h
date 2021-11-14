#ifndef DG_LLVM_VALUE_RELATION_STRUCTURE_ANALYZER_HPP_
#define DG_LLVM_VALUE_RELATION_STRUCTURE_ANALYZER_HPP_

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <algorithm>

#include "GraphElements.h"
#include "dg/AnalysisOptions.h"

namespace dg {
namespace vr {

struct AllocatedSizeView {
    const llvm::Value *elementCount = nullptr;
    uint64_t elementSize = 0; // in bytes

    AllocatedSizeView() = default;
    AllocatedSizeView(const llvm::Value *count, uint64_t size)
            : elementCount(count), elementSize(size) {}
};

class AllocatedArea {
    const llvm::Value *ptr;
    // used only if memory was allocated with realloc, as fallback when realloc
    // fails
    const llvm::Value *reallocatedPtr = nullptr;
    AllocatedSizeView originalSizeView;

  public:
    static const llvm::Value *stripCasts(const llvm::Value *inst);

    static uint64_t getBytes(const llvm::Type *type);

    AllocatedArea(const llvm::AllocaInst *alloca);

    AllocatedArea(const llvm::CallInst *call);

    const llvm::Value *getPtr() const { return ptr; }
    const llvm::Value *getReallocatedPtr() const { return reallocatedPtr; }

    std::vector<AllocatedSizeView> getAllocatedSizeViews() const;

#ifndef NDEBUG
    void ddump() const;
#endif
};

struct CallRelation {
    std::vector<std::pair<const llvm::Value *, const llvm::Value *>> equalPairs;
    VRLocation *callSite = nullptr;
};

class StructureAnalyzer {
    const llvm::Module &module;
    VRCodeGraph &codeGraph;

    // holds vector of instructions, which are processed on any path back to
    // given location is computed only for locations with more than one
    // predecessor
    std::map<VRLocation *, std::vector<const llvm::Instruction *>> inloopValues;

    // holds vector of values, which are defined at given location
    std::map<VRLocation *, std::set<const llvm::Value *>> defined;

    const std::vector<unsigned> collected = {llvm::Instruction::Add,
                                             llvm::Instruction::Sub,
                                             llvm::Instruction::Mul};
    std::map<unsigned, std::set<const llvm::Instruction *>> instructionSets;

    std::vector<AllocatedArea> allocatedAreas;

    std::map<const llvm::Function *, std::vector<CallRelation>>
            callRelationsMap;

    void categorizeEdges();

    void findLoops();

    std::set<VREdge *> collectBackward(const llvm::Function &f,
                                       VRLocation &from);

    void initializeDefined();

    void collectInstructionSet();

    bool isValidAllocationCall(const llvm::Value *val) const;

    void collectAllocatedAreas();

    void setValidAreasFromNoPredecessors(std::vector<bool> &validAreas) const;

    std::pair<unsigned, const AllocatedArea *>
    getEqualArea(const ValueRelations &graph, const llvm::Value *ptr) const;

    void invalidateHeapAllocatedAreas(std::vector<bool> &validAreas) const;

    void setValidAreasByInstruction(VRLocation &location,
                                    std::vector<bool> &validAreas,
                                    VRInstruction *vrinst) const;

    void setValidArea(std::vector<bool> &validAreas, const AllocatedArea *area,
                      unsigned index, bool validateThis) const;

    // if heap allocation call was just checked as successful, mark memory valid
    void setValidAreasByAssumeBool(VRLocation &location,
                                   std::vector<bool> &validAreas,
                                   VRAssumeBool *assume) const;

    void
    setValidAreasFromSinglePredecessor(VRLocation &location,
                                       std::vector<bool> &validAreas) const;

    bool trueInAll(const std::vector<std::vector<bool>> &validInPreds,
                   unsigned index) const;

    // in returned vector, false signifies that corresponding area is
    // invalidated by some of the passed instructions
    std::vector<bool> getInvalidatedAreas(
            const std::vector<const llvm::Instruction *> &instructions) const;

    void
    setValidAreasFromMultiplePredecessors(VRLocation &location,
                                          std::vector<bool> &validAreas) const;

    void computeValidAreas() const;

    void initializeCallRelations();

  public:
    StructureAnalyzer(const llvm::Module &m, VRCodeGraph &g)
            : module(m), codeGraph(g){};

    void analyzeBeforeRelationsAnalysis();

    void analyzeAfterRelationsAnalysis();

    bool isDefined(VRLocation *loc, const llvm::Value *val) const;

    // assumes that location is valid loop start (join of tree and back edges)
    const std::vector<const llvm::Instruction *> &
    getInloopValues(VRLocation &location) const {
        return inloopValues.at(&location);
    }

    const std::set<const llvm::Instruction *> &
    getInstructionSetFor(unsigned opcode) const {
        return instructionSets.at(opcode);
    }

    std::pair<unsigned, const AllocatedArea *>
    getAllocatedAreaFor(const llvm::Value *ptr) const;

    unsigned getNumberOfAllocatedAreas() const { return allocatedAreas.size(); }

    const std::vector<CallRelation> &
    getCallRelationsFor(const llvm::Instruction *inst) const;
};

} // namespace vr
} // namespace dg

#endif // DG_LLVM_VALUE_RELATION_STRUCTURE_ANALYZER_HPP_
