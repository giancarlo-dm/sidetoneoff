// --diag: one attempt with no retries, and a report of everything found on the way (plan, section 5.5).
#pragma once

enum class DiagOutput {
    Dialog,   // a message box (Ctrl+C copies its text)
    Console,  // stdout, for scripts and agents
};

// Returns the process exit code: 0 when the headset reads back sidetone 0 after the set, otherwise 1.
int RunDiag(DiagOutput output);
