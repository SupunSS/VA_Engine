#pragma once
#include <memory>
#include <vector>

// Generic command pattern backing EditorUI's undo/redo. Not scoped to any
// one tool — terrain strokes, prop placement, prop deletion, and gizmo
// transform edits all push through the same CommandHistory (see
// EditorUI::m_commandHistory), per the Level Design System design doc's
// §9.1 decision to build this once rather than per-tool.
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Do() = 0;
    virtual void Undo() = 0;
    virtual const char* GetLabel() const = 0; // shown in the Edit menu, e.g. "Undo: Place Object"
};

class CommandHistory {
public:
    // Calls Do() immediately, then records it. Redo stack is cleared —
    // once you make a new edit, anything you'd previously undone is gone,
    // same convention as every editor with undo/redo.
    void Execute(std::unique_ptr<ICommand> command) {
        command->Do();
        m_undoStack.push_back(std::move(command));
        m_redoStack.clear();
        TrimIfNeeded();
    }

    void Undo() {
        if (m_undoStack.empty()) return;
        auto command = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        command->Undo();
        m_redoStack.push_back(std::move(command));
    }

    void Redo() {
        if (m_redoStack.empty()) return;
        auto command = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        command->Do();
        m_undoStack.push_back(std::move(command));
    }

    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }
    const char* PeekUndoLabel() const { return m_undoStack.empty() ? "" : m_undoStack.back()->GetLabel(); }
    const char* PeekRedoLabel() const { return m_redoStack.empty() ? "" : m_redoStack.back()->GetLabel(); }

    // Called on level load/unload — undoing past a scene reload would
    // operate on entities/handles that no longer exist.
    void Clear() { m_undoStack.clear(); m_redoStack.clear(); }

private:
    void TrimIfNeeded() {
        constexpr size_t kMaxHistory = 200; // unbounded history isn't worth the memory for an editor session
        while (m_undoStack.size() > kMaxHistory) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }

    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
};