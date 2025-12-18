#include "../include/StorageEngine.hpp"

// --- MAIN EXECUTION ---
int main() {
    cout << "===========================================" << endl;
    cout << "   MINI-DBMS STORAGE ENGINE STARTING...    " << endl;
    cout << "===========================================" << endl;

    StorageManager sm("database.db");       // Initialize disk storage file
    BufferManager bm(sm);                   // Initialize memory manager
    BPlusTree tree(bm);                     // Initialize the B+ Tree structure

    // Executing a sequence of inserts to demonstrate Buffer Hits, Misses, and Tree Splits
    tree.insert(10);                        // Simple insert
    tree.insert(20);                        // Simple insert
    tree.insert(30);                        // Fills the first leaf
    tree.insert(40);                        // TRIGGERS SPLIT: Root becomes Internal Node
    tree.insert(50);                        // TRIGGERS EVICTION: Page 0 or 1 will be kicked to disk

    cout << "\n===========================================" << endl;
    cout << "   DEMO COMPLETE: CHECK database.db FILE   " << endl;
    cout << "===========================================" << endl;
    return 0;                               // Program finish
}