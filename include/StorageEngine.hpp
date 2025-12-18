#ifndef STORAGE_ENGINE_HPP
#define STORAGE_ENGINE_HPP

#include <iostream>     // Provides standard input/output streams (cout)
#include <fstream>      // Provides file stream classes for binary disk I/O
#include <vector>       // Used to manage the collection of frames in the Buffer Pool
#include <unordered_map> // Used for the Page Table to achieve O(1) page lookups in RAM
#include <list>         // Used to implement the LRU (Least Recently Used) tracking list
#include <cstring>      // Provides memory manipulation functions like memset and memcpy
#include <algorithm>    // Provides the sort function used during B+ Tree node splitting

using namespace std;    // Allows using standard library members without the std:: prefix

// --- GLOBAL SYSTEM CONFIGURATIONS ---
const int PAGE_SIZE = 4096;        // 4KB: The standard block size for disk/RAM data transfer
const int BUFFER_CAPACITY = 3;     // Limits RAM to 3 pages to force eviction logic visibility
const int MAX_KEYS = 3;            // Max keys per node; small value triggers splits quickly

// --- STORAGE MANAGER (DISK LAYER) ---
// Manages the physical byte-offsets within the binary database file
class StorageManager {
    string fileName;               // The string name of the database file on disk
    fstream dbFile;                // The actual stream object for reading and writing bits
public:
    StorageManager(string name) : fileName(name) {
        // Open file: in/out permission, binary mode, and trunc (wipes file for a clean demo)
        dbFile.open(fileName, ios::in | ios::out | ios::binary | ios::trunc);
        if (!dbFile.is_open()) {   // If the file fails to open (doesn't exist):
            dbFile.open(fileName, ios::out | ios::binary); // Create the file using 'out'
            dbFile.close();        // Close it immediately to reset the stream state
            dbFile.open(fileName, ios::in | ios::out | ios::binary); // Re-open for R/W access
        }
    }
    void writeDisk(int pageID, const char* data) {
        cout << "[DISK] Writing Page " << pageID << " to " << fileName << "..." << endl;
        dbFile.seekp(pageID * PAGE_SIZE, ios::beg); // Move "put" pointer to (pageID * 4096)
        dbFile.write(data, PAGE_SIZE);             // Write 4096 raw bytes to the disk
        dbFile.flush();                            // Force the OS to physically write to the drive
    }
    void readDisk(int pageID, char* buffer) {
        cout << "[DISK] Reading Page " << pageID << " from disk..." << endl;
        dbFile.seekg(pageID * PAGE_SIZE, ios::beg); // Move "get" pointer to start of PageID
        dbFile.read(buffer, PAGE_SIZE);            // Read 4096 bytes into the RAM buffer
    }
};

// --- BUFFER MANAGER (RAM LAYER) ---
// Structure representing a slot in RAM
struct Frame {
    int pageID = -1;               // ID of the page currently in RAM; -1 indicates empty
    bool dirty = false;            // Flag: True if data was modified but not yet saved to disk
    char data[PAGE_SIZE];          // The actual 4096-byte memory buffer
};

class BufferManager {
    StorageManager& sm;            // Reference to the Storage Layer for Disk I/O
    vector<Frame> pool;            // The Buffer Pool: a vector of RAM frames
    unordered_map<int, int> pageTable; // Fast mapping: PageID -> index in 'pool' vector
    list<int> lru;                 // Tracking usage: Front is Newest, Back is Oldest

public:
    int nextPageID = 0;            // Counter to assign unique IDs to new database pages
    BufferManager(StorageManager& s) : sm(s), pool(BUFFER_CAPACITY) {}

    char* fetchPage(int pageID) {
        if (pageTable.count(pageID)) { // CASE: Page is already in RAM (Buffer Hit)
            cout << "[BUFFER] Hit! Page " << pageID << " found in RAM." << endl;
            touch(pageTable[pageID]);  // Update its priority to "Most Recently Used"
            return pool[pageTable[pageID]].data; // Return pointer to the data
        }
        // CASE: Page is not in RAM (Buffer Miss)
        cout << "[BUFFER] Miss! Page " << pageID << " not in RAM." << endl;
        int frameIdx = evict();        // Find or create a free frame in RAM
        sm.readDisk(pageID, pool[frameIdx].data); // Pull the page from disk into RAM
        pool[frameIdx].pageID = pageID; // Update metadata for this frame
        pool[frameIdx].dirty = false;   // Reset dirty flag as it matches disk content
        pageTable[pageID] = frameIdx;   // Update Page Table with new location
        touch(frameIdx);                // Update LRU list
        return pool[frameIdx].data;     // Return data pointer
    }

    int allocatePage() {
        int pid = nextPageID++;         // Generate a new unique Page ID
        cout << "[SYSTEM] Allocating new Page " << pid << endl;
        char* p = fetchPage(pid);       // Bring the new page into the buffer
        memset(p, 0, PAGE_SIZE);        // Initialize the new page with zeros
        markDirty(pid);                 // Ensure it gets saved to disk later
        return pid;                     // Return the ID for the B+ tree to use
    }

    // Set dirty flag to true when the B+ Tree modifies a page
    void markDirty(int pageID) { if(pageTable.count(pageID)) pool[pageTable[pageID]].dirty = true; }

    int evict() {
        if (lru.size() < BUFFER_CAPACITY) return lru.size(); // Use next empty slot if available
        int idx = lru.back();           // Pick the least recently used frame as the "victim"
        cout << "[EVICT] Buffer full. Kicking out Page " << pool[idx].pageID << " (LRU Policy)." << endl;
        if (pool[idx].dirty) sm.writeDisk(pool[idx].pageID, pool[idx].data); // Save if modified
        pageTable.erase(pool[idx].pageID); // Remove the evicted page from the lookup map
        lru.pop_back();                 // Remove from the tracking list
        return idx;                     // Return the index for re-use
    }

    void touch(int idx) {
        lru.remove(idx);                // Find the frame index in the list
        lru.push_front(idx);            // Move it to the front (Most Recently Used)
    }
};

// --- B+ TREE NODE STRUCTURE ---
// Binary layout of a B+ tree node inside a 4KB page
struct BPlusNode {
    bool isLeaf;                   // Flag: True for leaf nodes, False for internal nodes
    int numKeys;                   // Current number of keys stored in this node
    int parentPage;                // PageID of the parent node (for upward traversal)
    int keys[MAX_KEYS];            // Sorted array of integer keys
    int children[MAX_KEYS + 1];    // Pointers (PageIDs) to child nodes or data
    int nextLeaf;                  // Linked list pointer to the next leaf sibling
};

class BPlusTree {
    BufferManager& bm;             // Access to the memory management layer
    int rootPage;                  // The PageID of the top-most node (Root)

public:
    BPlusTree(BufferManager& b) : bm(b) {
        rootPage = bm.allocatePage();  // Initialize the tree with a root page
        BPlusNode* root = (BPlusNode*)bm.fetchPage(rootPage); // Cast bytes to Node struct
        root->isLeaf = true;           // Every new tree starts with the root as a leaf
        root->numKeys = 0;             // Root starts empty
        root->parentPage = -1;         // Root has no parent
    }

    void insert(int key) {
        cout << "\n>>> USER COMMAND: INSERT " << key << " <<<" << endl;
        int leafPage = findLeaf(rootPage, key); // Find the leaf where the key belongs
        insertIntoLeaf(leafPage, key);          // Execute the leaf insertion logic
    }

    int findLeaf(int currPage, int key) {
        BPlusNode* node = (BPlusNode*)bm.fetchPage(currPage); // Load node from buffer
        if (node->isLeaf) return currPage;                   // If it's a leaf, return its ID
        int i = 0;                                           // Navigation logic:
        while (i < node->numKeys && key >= node->keys[i]) i++; // Find correct child pointer
        return findLeaf(node->children[i], key);             // Recurse down the tree
    }

    void insertIntoLeaf(int pageID, int key) {
        BPlusNode* node = (BPlusNode*)bm.fetchPage(pageID);  // Get leaf from buffer
        if (node->numKeys < MAX_KEYS) {                      // If room exists:
            int i = node->numKeys - 1;                       // Shift keys to maintain order
            while (i >= 0 && node->keys[i] > key) {
                node->keys[i + 1] = node->keys[i];
                i--;
            }
            node->keys[i + 1] = key;                         // Insert the new key
            node->numKeys++;                                 // Update key count
            bm.markDirty(pageID);                            // Mark page for disk write
            cout << "[TREE] Key " << key << " placed in Leaf Page " << pageID << endl;
        } else {
            splitLeaf(pageID, key);                          // Node full: trigger split
        }
    }

    void splitLeaf(int oldPageID, int key) {
        cout << "[TREE] Node full! Initiating B+ Tree Split Logic..." << endl;
        int newPageID = bm.allocatePage();   // Allocate new sibling page
        BPlusNode* oldNode = (BPlusNode*)bm.fetchPage(oldPageID); // Get old node
        BPlusNode* newNode = (BPlusNode*)bm.fetchPage(newPageID); // Get new sibling

        vector<int> tempKeys(oldNode->keys, oldNode->keys + MAX_KEYS); // Copy existing keys
        tempKeys.push_back(key);             // Add the new key to the set
        sort(tempKeys.begin(), tempKeys.end()); // Sort the overflowed set

        newNode->isLeaf = true;              // Sibling is a leaf
        int mid = (MAX_KEYS + 1) / 2;        // Determine split point (half-full)
        oldNode->numKeys = mid;              // Assign first half to old node
        newNode->numKeys = (MAX_KEYS + 1) - mid; // Assign second half to new node

        for (int i = 0; i < oldNode->numKeys; i++) oldNode->keys[i] = tempKeys[i]; // Copy first half
        for (int i = 0; i < newNode->numKeys; i++) newNode->keys[i] = tempKeys[mid + i]; // Copy second half

        bm.markDirty(oldPageID);             // Save changes to old node
        bm.markDirty(newPageID);             // Save changes to new sibling
        cout << "[TREE] Split complete. New Leaf Page " << newPageID << " created." << endl;
        
        insertIntoParent(oldPageID, newNode->keys[0], newPageID); // Push middle key to parent
    }

    void insertIntoParent(int left, int key, int right) {
        if (left == rootPage) {              // If root was split, create new root
            int newRoot = bm.allocatePage(); // Get page for new top node
            BPlusNode* r = (BPlusNode*)bm.fetchPage(newRoot); // Get struct pointer
            r->isLeaf = false;               // New root is an Internal Node
            r->keys[0] = key;                // Store the promoted key
            r->children[0] = left;           // Left pointer points to old root
            r->children[1] = right;          // Right pointer points to new sibling
            r->numKeys = 1;                  // New root starts with 1 key
            rootPage = newRoot;              // Update tree root ID
            cout << "[TREE] New Root created (Page " << newRoot << "). Tree height increased!" << endl;
        }
    }
};

#endif