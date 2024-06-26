#ifndef AVLTREE_H
#define AVLTREE_H
#include "ORAM.hpp"
#include <functional>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <array>
#include <memory>
#include <type_traits>
#include <iomanip>
#include "Bid.h"
#include <random>
#include "sgx_trts.h"
#include <algorithm>
#include <stdlib.h>
using namespace std;

// void check_memory2(string text)
// {
//     unsigned int required = 0x4f00000; // adapt to native uint
//     char *mem = NULL;
//     while (mem == NULL)
//     {
//         mem = (char *)malloc(required);
//         if ((required -= 8) < 0xFFF)
//         {
//             if (mem)
//                 free(mem);
//             printf("Cannot allocate enough memory\n");
//             return;
//         }
//     }

//     free(mem);
//     mem = (char *)malloc(required);
//     if (mem == NULL)
//     {
//         printf("Cannot enough allocate memory\n");
//         return;
//     }
//     printf("%s = %d\n", text.c_str(), required);
//     free(mem);
// }

template <typename T>
class AVLTree
{
private:
    ORAM<T> *oram;
    int maxOfRandom;
    bool doubleRotation;

    int max(int a, int b)
    {
        int res = CTcmp(a, b);
        return CTeq(res, 1) ? a : b;
    }

    Node<T> *newNode(Bid omapKey, vector<byte_t> value)
    {
        Node<T> *node = new Node<T>();
        node->key = omapKey;
        node->index = index++;
        std::fill(node->value.begin(), node->value.end(), 0);
        std::copy(value.begin(), value.end(), node->value.begin());
        node->leftID = 0;
        node->leftPos = -1;
        node->rightPos = -1;
        node->rightID = 0;
        node->pos = 0;
        node->isDummy = false;
        node->height = 1; // new node is initially added at leaf
        return node;
    }

    Node<T> *newNode(Bid omapKey)
    {
        Node<T> *node = new Node<T>();
        node->key = omapKey;
        node->index = index++;
        std::fill(node->value.begin(), node->value.end(), 0);
        node->leftID = 0;
        node->leftPos = -1;
        node->rightPos = -1;
        node->rightID = 0;
        node->pos = 0;
        node->isDummy = false;
        node->height = 1; // new node is initially added at leaf
        return node;
    }

    void rotate(Node<T> *node, Node<T> *oppositeNode, int targetHeight, bool right, bool dummyOp)
    {
        Node<T> *T2 = nullptr;
        Node<T> *tmpDummyNode = new Node<T>();
        tmpDummyNode->isDummy = true;
        T2 = newNode(0);
        Node<T> *tmp = nullptr;
        bool left = !right;
        unsigned long long newPos = RandomPath();
        unsigned long long dumyPos;
        Bid dummy;
        dummy.setValue(oram->nextDummyCounter++);

        bool cond1 = !dummyOp && ((right && oppositeNode->rightID.isZero()) || (left && oppositeNode->leftID.isZero()));
        bool cond2 = !dummyOp && right;
        bool cond3 = !dummyOp && left;

        T2->height = Node<T>::conditional_select(0, T2->height, cond1);

        Bid readKey = dummy;
        readKey = Bid::conditional_select(oppositeNode->rightID, readKey, !cond1 && cond2);
        readKey = Bid::conditional_select(oppositeNode->leftID, readKey, !cond1 && !cond2 && cond3);

        bool isDummy = !(!cond1 && (cond2 || cond3));

        tmp = readWriteCacheNode(readKey, tmpDummyNode, true, isDummy); // READ

        Node<T>::conditional_assign(T2, tmp, !cond1 && (cond2 || cond3));
        delete tmp;

        // Perform rotation
        cond1 = !dummyOp && right;
        cond2 = !dummyOp;
        oppositeNode->rightID = Bid::conditional_select(node->key, oppositeNode->rightID, cond1);
        oppositeNode->rightPos = Bid::conditional_select(node->pos, oppositeNode->rightPos, cond1);
        node->leftID = Bid::conditional_select(T2->key, node->leftID, cond1);
        node->leftPos = Bid::conditional_select(T2->pos, node->leftPos, cond1);

        oppositeNode->leftID = Bid::conditional_select(node->key, oppositeNode->leftID, !cond1 && cond2);
        oppositeNode->leftPos = Bid::conditional_select(node->pos, oppositeNode->leftPos, !cond1 && cond2);
        node->rightID = Bid::conditional_select(T2->key, node->rightID, !cond1 && cond2);
        node->rightPos = Bid::conditional_select(T2->pos, node->rightPos, !cond1 && cond2);

        // Update heights
        int curNodeHeight = max(T2->height, targetHeight) + 1;
        node->height = Node<T>::conditional_select(curNodeHeight, node->height, !dummyOp);

        int oppositeOppositeHeight = 0;
        unsigned long long newOppositePos = RandomPath();
        Node<T> *oppositeOppositeNode = nullptr;

        cond1 = !dummyOp && right && !oppositeNode->leftID.isZero();
        cond2 = !dummyOp && left && !oppositeNode->rightID.isZero();

        readKey = dummy;
        readKey = Bid::conditional_select(oppositeNode->leftID, readKey, cond1);
        readKey = Bid::conditional_select(oppositeNode->rightID, readKey, !cond1 && cond2);

        isDummy = !(cond1 || cond2);

        oppositeOppositeNode = readWriteCacheNode(readKey, tmpDummyNode, true, isDummy); // READ

        oppositeOppositeHeight = Node<T>::conditional_select(oppositeOppositeNode->height, oppositeOppositeHeight, cond1 || cond2);
        delete oppositeOppositeNode;

        int maxValue = max(oppositeOppositeHeight, curNodeHeight) + 1;
        oppositeNode->height = Node<T>::conditional_select(maxValue, oppositeNode->height, !dummyOp);

        delete T2;
        delete tmpDummyNode;
    }

    void rotate2(Node<T> *node, Node<T> *oppositeNode, int targetHeight, bool right, bool dummyOp)
    {
        Node<T> *T2 = nullptr;
        Node<T> *tmpDummyNode = new Node<T>();
        tmpDummyNode->isDummy = true;
        T2 = newNode(0);
        Node<T> *tmp = nullptr;
        bool left = !right;
        unsigned long long newPos = RandomPath();
        unsigned long long dumyPos;
        Bid dummy;
        dummy.setValue(oram->nextDummyCounter++);

        if (!dummyOp && ((right && oppositeNode->rightID == 0) || (left && oppositeNode->leftID == 0)))
        {
            T2->height = 0;
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
        }
        else if (!dummyOp && right)
        {
            T2 = readWriteCacheNode(oppositeNode->rightID, tmpDummyNode, true, false);
        }
        else if (!dummyOp && left)
        {
            T2 = readWriteCacheNode(oppositeNode->leftID, tmpDummyNode, true, false);
        }
        else
        {
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
        }
        // Perform rotation
        if (!dummyOp && right)
        {
            oppositeNode->rightID = node->key;
            oppositeNode->rightPos = node->pos;
            oppositeNode->leftID = oppositeNode->leftID;
            oppositeNode->leftPos = oppositeNode->leftPos;
            node->rightID = node->rightID;
            node->rightPos = node->rightPos;
            node->leftID = T2->key;
            node->leftPos = T2->pos;
        }
        else if (!dummyOp)
        {
            oppositeNode->leftID = node->key;
            oppositeNode->leftPos = node->pos;
            oppositeNode->rightID = oppositeNode->rightID;
            oppositeNode->rightPos = oppositeNode->rightPos;
            node->rightID = T2->key;
            node->rightPos = T2->pos;
            node->leftID = node->leftID;
            node->leftPos = node->leftPos;
        }
        else
        {
            oppositeNode->leftID = oppositeNode->leftID;
            oppositeNode->leftPos = oppositeNode->leftPos;
            oppositeNode->rightID = oppositeNode->rightID;
            oppositeNode->rightPos = oppositeNode->rightPos;
            node->rightID = node->rightID;
            node->rightPos = node->rightPos;
            node->leftID = node->leftID;
            node->leftPos = node->leftPos;
        }
        // Update heights
        int curNodeHeight = max(T2->height, targetHeight) + 1;
        if (!dummyOp)
        {
            node->height = curNodeHeight;
        }
        else
        {
            node->height = node->height;
        }

        int oppositeOppositeHeight = 0;
        unsigned long long newOppositePos = RandomPath();
        Node<T> *oppositeOppositeNode = nullptr;
        if (!dummyOp && right && oppositeNode->leftID != 0)
        {
            oppositeOppositeNode = readWriteCacheNode(oppositeNode->leftID, tmpDummyNode, true, false);
            oppositeOppositeHeight = oppositeOppositeNode->height;
            delete oppositeOppositeNode;
        }
        else if (!dummyOp && left && oppositeNode->rightID != 0)
        {
            oppositeOppositeNode = readWriteCacheNode(oppositeNode->rightID, tmpDummyNode, true, false);
            oppositeOppositeHeight = oppositeOppositeNode->height;
            delete oppositeOppositeNode;
        }
        else
        {
            oppositeOppositeNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            oppositeOppositeHeight = oppositeOppositeHeight;
            delete oppositeOppositeNode;
        }
        int maxValue = max(oppositeOppositeHeight, curNodeHeight) + 1;
        if (!dummyOp)
        {
            oppositeNode->height = maxValue;
        }
        else
        {
            oppositeNode->height = oppositeNode->height;
        }
        delete T2;
        delete tmpDummyNode;
    }

    unsigned long long RandomPath()
    {
        uint32_t val;
        sgx_read_rand((unsigned char *)&val, 4);
        return val % (maxOfRandom);
    }

    /**
     * constant time comparator
     * @param left
     * @param right
     * @return left < right -> -1,  left = right -> 0, left > right -> 1
     */
    static int CTcmp(long long lhs, long long rhs)
    {
        unsigned __int128 overflowing_iff_lt = (__int128)lhs - (__int128)rhs;
        unsigned __int128 overflowing_iff_gt = (__int128)rhs - (__int128)lhs;
        int is_less_than = (int)-(overflowing_iff_lt >> 127);   // -1 if self < other, 0 otherwise.
        int is_greater_than = (int)(overflowing_iff_gt >> 127); // 1 if self > other, 0 otherwise.
        int result = is_less_than + is_greater_than;
        return result;
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static long long conditional_select(long long a, long long b, int choice)
    {
        unsigned long long one = 1;
        return (~((unsigned long long)choice - one) & a) | ((unsigned long long)(choice - one) & b);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static int conditional_select(int a, int b, int choice)
    {
        unsigned int one = 1;
        return (~((unsigned int)choice - one) & a) | ((unsigned int)(choice - one) & b);
    }

    static bool CTeq(int a, int b)
    {
        return !(a ^ b);
    }

    static bool CTeq(long long a, long long b)
    {
        return !(a ^ b);
    }

    int sortedArrayToBST(vector<Node<T> *> *nodes, long long start, long long end, unsigned long long &pos, Bid &node, map<unsigned long long, unsigned long long> *permutation)
    {
        if (start > end)
        {
            pos = -1;
            node = 0;
            return 0;
        }

        unsigned long long mid = (start + end) / 2;
        Node<T> *root = (*nodes)[mid];

        int leftHeight = sortedArrayToBST(nodes, start, mid - 1, root->leftPos, root->leftID, permutation);

        int rightHeight = sortedArrayToBST(nodes, mid + 1, end, root->rightPos, root->rightID, permutation);
        root->pos = (*permutation)[permutationIterator];
        permutationIterator++;
        root->height = max(leftHeight, rightHeight) + 1;
        pos = root->pos;
        node = root->key;
        return root->height;
    }
    unsigned long long permutationIterator = 0;
    unsigned long long INF = 92233720368547758;

    Node<T> *readWriteCacheNode(Bid bid, Node<T> *inputnode, bool isRead, bool isDummy)
    {
        Node<T> *tmpWrite = Node<T>::clone(inputnode);

        Node<T> *res = new Node<T>();
        res->isDummy = true;
        res->index = oram->nextDummyCounter++;
        res->key = oram->nextDummyCounter++;
        bool write = !isRead;

        for (Node<T> *node : avlCache)
        {
            bool match = Node<T>::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;

            node->isDummy = Node<T>::conditional_select(true, node->isDummy, !isDummy && match && write);
            bool choice = !isDummy && match && isRead && !node->isDummy;
            res->index = Node<T>::conditional_select((long long)node->index, (long long)res->index, choice);
            res->isDummy = Node<T>::conditional_select(node->isDummy, res->isDummy, choice);
            res->pos = Node<T>::conditional_select((long long)node->pos, (long long)res->pos, choice);
            for (int k = 0; k < res->value.size(); k++)
            {
                res->value[k] = Node<T>::conditional_select(node->value[k], res->value[k], choice);
            }
            res->evictionNode = Node<T>::conditional_select(node->evictionNode, res->evictionNode, choice);
            res->modified = Node<T>::conditional_select(true, res->modified, choice);
            res->height = Node<T>::conditional_select(node->height, res->height, choice);
            res->leftPos = Node<T>::conditional_select(node->leftPos, res->leftPos, choice);
            res->rightPos = Node<T>::conditional_select(node->rightPos, res->rightPos, choice);
            for (int k = 0; k < res->key.id.size(); k++)
            {
                res->key.id[k] = Node<T>::conditional_select(node->key.id[k], res->key.id[k], choice);
            }
            for (int k = 0; k < res->leftID.id.size(); k++)
            {
                res->leftID.id[k] = Node<T>::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
            }
            for (int k = 0; k < res->rightID.id.size(); k++)
            {
                res->rightID.id[k] = Node<T>::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
            }
        }

        avlCache.push_back(tmpWrite);
        return res;
    }

    void bitonic_sort(vector<Node<T> *> *nodes, int low, int n, int dir)
    {
        if (n > 1)
        {
            int middle = n / 2;
            bitonic_sort(nodes, low, middle, !dir);
            bitonic_sort(nodes, low + middle, n - middle, dir);
            bitonic_merge(nodes, low, n, dir);
        }
    }

    void bitonic_merge(vector<Node<T> *> *nodes, int low, int n, int dir)
    {
        if (n > 1)
        {
            int m = greatest_power_of_two_less_than(n);

            for (int i = low; i < (low + n - m); i++)
            {
                if (i != (i + m))
                {
                    compare_and_swap((*nodes)[i], (*nodes)[i + m], dir);
                }
            }

            bitonic_merge(nodes, low, m, dir);
            bitonic_merge(nodes, low + m, n - m, dir);
        }
    }

    void compare_and_swap(Node<T> *item_i, Node<T> *item_j, int dir)
    {
        int res = Bid::CTcmp(item_i->key, item_j->key);
        int cmp = Node<T>::CTeq(res, 1);
        Node<T>::conditional_swap(item_i, item_j, Node<T>::CTeq(cmp, dir));
    }

    void bitonicSort(vector<Node<T> *> *nodes)
    {
        int len = nodes->size();
        bitonic_sort(nodes, 0, len, 1);
    }

    int greatest_power_of_two_less_than(int n)
    {
        int k = 1;
        while (k > 0 && k < n)
        {
            k = k << 1;
        }
        return k >> 1;
    }

public:
    // AVLTree(long long maxSize, bytes<Key> secretkey, Bid &rootKey, unsigned long long &rootPos, map<Bid, vector<byte_t>> *pairs, map<unsigned long long, unsigned long long> *permutation)
    // {
    //     int depth = (int)(ceil(log2(maxSize)) - 1) + 1;
    //     maxOfRandom = (long long)(pow(2, depth));
    //     times.push_back(vector<double>());
    //     times.push_back(vector<double>());
    //     times.push_back(vector<double>());
    //     times.push_back(vector<double>());

    //     vector<Node<T> *> nodes;
    //     for (auto pair : (*pairs))
    //     {
    //         Node<T> *node = newNode(pair.first, pair.second);
    //         nodes.push_back(node);
    //     }

    //     int nextPower2 = (int)pow(2, ceil(log2(nodes.size())));
    //     for (int i = (int)nodes.size(); i < nextPower2; i++)
    //     {
    //         Bid bid = INF + i;
    //         Node<T> *node = newNode(bid);
    //         node->isDummy = false;
    //         nodes.push_back(node);
    //     }

    //     bitonicSort(&nodes);
    //     double t;
    //     printf("Creating BST of %d Nodes\n", nodes.size());
    //     ocall_start_timer(53);
    //     sortedArrayToBST(&nodes, 0, nodes.size() - 2, rootPos, rootKey, permutation);
    //     ocall_stop_timer(&t, 53);
    //     times[0].push_back(t);
    //     printf("Inserting in ORAM\n");

    //     double gp;
    //     int size = (int)nodes.size();
    //     for (int i = size; i < maxOfRandom * Z; i++)
    //     {
    //         Bid bid;
    //         bid = INF + i;
    //         Node<T> *node = newNode(bid);
    //         node->isDummy = true;
    //         node->pos = (*permutation)[permutationIterator];
    //         permutationIterator++;
    //         nodes.push_back(node);
    //     }
    //     ocall_start_timer(53);
    //     oram = new ORAM<T>(maxSize, secretkey, &nodes);
    //     ocall_stop_timer(&t, 53);
    //     times[1].push_back(t);
    // }

    AVLTree(long long maxSize, bytes<Key> secretkey, bool isEmptyMap)
    {
        oram = new ORAM<T>(true, maxSize, secretkey, false, isEmptyMap);
        int depth = (int)(ceil(log2(maxSize)) - 1) + 1;
        maxOfRandom = (long long)(pow(2, depth));
        times.push_back(vector<double>());
        times.push_back(vector<double>());
        times.push_back(vector<double>());
        times.push_back(vector<double>());
    }

    virtual ~AVLTree()
    {
        delete oram;
    }

    int totheight = 0;
    bool logTime = false;
    vector<vector<double>> times;
    bool exist;
    vector<Node<T> *> avlCache;
    unsigned long long index = 1;

    Bid insert(Bid rootKey, unsigned long long &rootPos, Bid omapKey, vector<byte_t> value, int &height, Bid lastID, bool isDummyIns)
    {
        totheight++;
        unsigned long long rndPos = RandomPath();
        std::array<byte_t, sizeof(T)> tmpval;
        std::fill(tmpval.begin(), tmpval.end(), 0);
        std::copy(value.begin(), value.end(), tmpval.begin());
        Node<T> *tmpDummyNode = new Node<T>();
        tmpDummyNode->isDummy = true;
        Bid dummy;
        Bid retKey;
        unsigned long long dumyPos;
        bool remainerIsDummy = false;
        dummy.setValue(oram->nextDummyCounter++);

        if (isDummyIns && CTeq(CTcmp(totheight, oram->depth * 1.44), 1))
        {
            Node<T> *nnode = newNode(omapKey, value);
            nnode->pos = RandomPath();
            height = Node<T>::conditional_select(nnode->height, height, !exist);
            rootPos = Node<T>::conditional_select(nnode->pos, rootPos, !exist);

            Node<T> *previousNode = readWriteCacheNode(omapKey, tmpDummyNode, true, !exist);

            Node<T> *wrtNode = Node<T>::clone(previousNode);
            Node<T>::conditional_assign(wrtNode, nnode, !exist);

            unsigned long long lastPos = nnode->pos;
            lastPos = Node<T>::conditional_select(previousNode->pos, lastPos, exist);

            Node<T> *tmp = oram->ReadWrite(omapKey, wrtNode, lastPos, nnode->pos, false, false, false);

            wrtNode->pos = nnode->pos;
            Node<T> *tmp2 = readWriteCacheNode(omapKey, wrtNode, false, false);
            Bid retKey = lastID;
            retKey = Bid::conditional_select(nnode->key, retKey, !exist);
            delete tmp;
            delete tmp2;
            delete wrtNode;
            delete nnode;
            delete previousNode;
            delete tmpDummyNode;
            return retKey;
        }
        /* 1. Perform the normal BST rotation */
        double t;
        bool cond1, cond2, cond3, cond4, cond5, isDummy;

        cond1 = !isDummyIns && !rootKey.isZero();
        remainerIsDummy = !cond1;

        Node<T> *node = nullptr;

        Bid initReadKey = dummy;
        initReadKey = Bid::conditional_select(rootKey, initReadKey, !remainerIsDummy);
        isDummy = remainerIsDummy;
        node = oram->ReadWrite(initReadKey, tmpDummyNode, rootPos, rootPos, true, isDummy, tmpval, Bid::CTeq(Bid::CTcmp(rootKey, omapKey), 0), true); // READ
        Node<T> *tmp2 = readWriteCacheNode(initReadKey, node, false, isDummy);
        delete tmp2;

        int balance = -1;
        int leftHeight = -1;
        int rightHeight = -1;
        Node<T> *leftNode = new Node<T>();
        bool leftNodeisNull = true;
        Node<T> *rightNode = new Node<T>();
        bool rightNodeisNull = true;
        std::array<byte_t, sizeof(T)> garbage;
        bool childDirisLeft = false;

        unsigned long long newRLPos = RandomPath();

        cond1 = !remainerIsDummy;
        int cmpRes = Bid::CTcmp(omapKey, node->key);
        cond2 = Node<T>::CTeq(cmpRes, -1);
        bool cond2_1 = node->rightID.isZero();
        cond3 = Node<T>::CTeq(cmpRes, 1);
        bool cond3_1 = node->leftID.isZero();
        cond4 = Node<T>::CTeq(cmpRes, 0);

        Bid insRootKey = rootKey;
        insRootKey = Bid::conditional_select(node->leftID, insRootKey, cond1 && cond2);
        insRootKey = Bid::conditional_select(node->rightID, insRootKey, cond1 && cond3);

        unsigned long long insRootPos = rootPos;
        insRootPos = Node<T>::conditional_select(node->leftPos, insRootPos, cond1 && cond2);
        insRootPos = Node<T>::conditional_select(node->rightPos, insRootPos, cond1 && cond3);

        int insHeight = height;
        insHeight = Node<T>::conditional_select(leftHeight, insHeight, cond1 && cond2);
        insHeight = Node<T>::conditional_select(rightHeight, insHeight, cond1 && cond3);

        Bid insLastID = dummy;
        insLastID = Bid::conditional_select(node->key, insLastID, (cond1 && cond4) || (!cond1));

        isDummy = (!cond1) || (cond1 && cond4);

        exist = Node<T>::conditional_select(true, exist, cond1 && cond4);
        Bid resValBid = insert(insRootKey, insRootPos, omapKey, value, insHeight, insLastID, isDummy);

        node->leftID = Bid::conditional_select(resValBid, node->leftID, cond1 && cond2);
        node->rightID = Bid::conditional_select(resValBid, node->rightID, cond1 && cond3);

        node->leftPos = Node<T>::conditional_select(insRootPos, node->leftPos, cond1 && cond2);
        node->rightPos = Node<T>::conditional_select(insRootPos, node->rightPos, cond1 && cond3);
        rootPos = Node<T>::conditional_select(insRootPos, rootPos, (!cond1) || (cond1 && cond4));

        leftHeight = Node<T>::conditional_select(insHeight, leftHeight, cond1 && cond2);
        rightHeight = Node<T>::conditional_select(insHeight, rightHeight, cond1 && cond3);
        height = Node<T>::conditional_select(insHeight, height, (!cond1) || (cond1 && cond4));

        totheight--;
        childDirisLeft = (cond1 && cond2);

        Bid readKey = dummy;
        readKey = Bid::conditional_select(node->rightID, readKey, cond1 && cond2 && (!cond2_1));
        readKey = Bid::conditional_select(node->leftID, readKey, cond1 && cond3 && (!cond3_1));

        unsigned long long tmpreadPos = newRLPos;
        tmpreadPos = Bid::conditional_select(node->rightPos, tmpreadPos, cond1 && cond2 && (!cond2_1));
        tmpreadPos = Bid::conditional_select(node->leftPos, tmpreadPos, cond1 && cond3 && (!cond3_1));

        isDummy = !((cond1 && cond2 && (!cond2_1)) || (cond1 && cond3 && (!cond3_1)));

        Node<T> *tmp = oram->ReadWrite(readKey, tmpDummyNode, tmpreadPos, tmpreadPos, true, isDummy, true); // READ
        Node<T>::conditional_assign(rightNode, tmp, cond1 && cond2 && (!cond2_1));
        Node<T>::conditional_assign(leftNode, tmp, cond1 && cond3 && (!cond3_1));

        tmp2 = readWriteCacheNode(tmp->key, tmp, false, isDummy);

        delete tmp;
        delete tmp2;

        tmp = readWriteCacheNode(node->key, tmpDummyNode, true, !(cond1 && cond4));

        Node<T>::conditional_assign(node, tmp, cond1 && cond4);

        delete tmp;

        rightNodeisNull = Node<T>::conditional_select(false, rightNodeisNull, cond1 && cond2 && (!cond2_1));
        leftNodeisNull = Node<T>::conditional_select(false, leftNodeisNull, cond1 && cond3 && (!cond3_1));

        rightHeight = Node<T>::conditional_select(0, rightHeight, cond1 && cond2 && cond2_1);
        rightHeight = Node<T>::conditional_select(rightNode->height, rightHeight, cond1 && cond2 && (!cond2_1));
        leftHeight = Node<T>::conditional_select(0, leftHeight, cond1 && cond3 && cond3_1);
        leftHeight = Node<T>::conditional_select(leftNode->height, leftHeight, cond1 && cond3 && (!cond3_1));

        std::fill(garbage.begin(), garbage.end(), 0);
        std::copy(value.begin(), value.end(), garbage.begin());

        for (int i = 0; i < garbage.size(); i++)
        {
            node->value[i] = Bid::conditional_select(garbage[i], node->value[i], cond1 && cond4);
        }

        rootPos = Node<T>::conditional_select(node->pos, rootPos, cond1 && cond4);
        height = Node<T>::conditional_select(node->height, height, cond1 && cond4);

        retKey = Bid::conditional_select(node->key, retKey, cond1 && cond4);
        retKey = Bid::conditional_select(resValBid, retKey, !cond1);

        remainerIsDummy = Node<T>::conditional_select(true, remainerIsDummy, cond1 && cond4);
        remainerIsDummy = Node<T>::conditional_select(false, remainerIsDummy, cond1 && (cond2 || cond3));

        /* 2. Update height of this ancestor node */

        int tmpHeight = max(leftHeight, rightHeight) + 1;
        node->height = Node<T>::conditional_select(node->height, tmpHeight, remainerIsDummy);
        height = Node<T>::conditional_select(height, node->height, remainerIsDummy);

        /* 3. Get the balance factor of this ancestor node to check whether this node became unbalanced */

        balance = Node<T>::conditional_select(balance, leftHeight - rightHeight, remainerIsDummy);

        ocall_start_timer(totheight + 100000);
        //------------------------------------------------

        cond1 = !remainerIsDummy && CTeq(CTcmp(balance, 1), 1) && Node<T>::CTeq(Bid::CTcmp(omapKey, node->leftID), -1);
        cond2 = !remainerIsDummy && CTeq(CTcmp(balance, -1), -1) && Node<T>::CTeq(Bid::CTcmp(omapKey, node->rightID), 1);
        cond3 = !remainerIsDummy && CTeq(CTcmp(balance, 1), 1) && Node<T>::CTeq(Bid::CTcmp(omapKey, node->leftID), 1);
        cond4 = !remainerIsDummy && CTeq(CTcmp(balance, -1), -1) && Node<T>::CTeq(Bid::CTcmp(omapKey, node->rightID), -1);
        cond5 = !remainerIsDummy;

        //  printf("tot:%d cond1:%d cond2:%d cond3:%d cond4:%d cond5:%d\n",totheight,cond1,cond2,cond3,cond4,cond5);

        //------------------------------------------------
        // Left/Right Node
        //------------------------------------------------

        Bid leftRightReadKey = dummy;
        leftRightReadKey = Bid::conditional_select(node->leftID, leftRightReadKey, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
        leftRightReadKey = Bid::conditional_select(node->rightID, leftRightReadKey, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);

        isDummy = !(((cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull) || (((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull));

        tmp = readWriteCacheNode(leftRightReadKey, tmpDummyNode, true, isDummy); // READ
        Node<T>::conditional_assign(leftNode, tmp, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
        Node<T>::conditional_assign(rightNode, tmp, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
        delete tmp;

        node->leftPos = Node<T>::conditional_select(leftNode->pos, node->leftPos, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
        leftNodeisNull = Node<T>::conditional_select(false, leftNodeisNull, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);

        node->rightPos = Node<T>::conditional_select(rightNode->pos, node->rightPos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
        rightNodeisNull = Node<T>::conditional_select(false, rightNodeisNull, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
        //------------------------------------------------
        // Left-Right Right-Left Node
        //------------------------------------------------

        Bid leftRightNodeReadKey = dummy;
        leftRightNodeReadKey = Bid::conditional_select(leftNode->rightID, leftRightNodeReadKey, !cond1 && !cond2 && cond3);
        leftRightNodeReadKey = Bid::conditional_select(rightNode->leftID, leftRightNodeReadKey, !cond1 && !cond2 && !cond3 && cond4);

        isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));

        tmp = readWriteCacheNode(leftRightNodeReadKey, tmpDummyNode, true, isDummy); // READ

        Node<T> *leftRightNode = new Node<T>();
        Node<T> *rightLeftNode = new Node<T>();

        Node<T>::conditional_assign(leftRightNode, tmp, (!cond1 && !cond2 && cond3));
        Node<T>::conditional_assign(rightLeftNode, tmp, (!cond1 && !cond2 && !cond3 && cond4));

        delete tmp;

        int leftLeftHeight = 0;
        int rightRightHeight = 0;

        Bid leftLeftRightRightKey = dummy;
        leftLeftRightRightKey = Bid::conditional_select(leftNode->leftID, leftLeftRightRightKey, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
        leftLeftRightRightKey = Bid::conditional_select(rightNode->rightID, leftLeftRightRightKey, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());

        isDummy = !(((!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero()) || ((!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero()));

        tmp = readWriteCacheNode(leftLeftRightRightKey, tmpDummyNode, true, isDummy); // READ

        Node<T> *leftLeftNode = new Node<T>();
        Node<T> *rightRightNode = new Node<T>();

        Node<T>::conditional_assign(leftLeftNode, tmp, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
        Node<T>::conditional_assign(rightRightNode, tmp, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
        delete tmp;

        leftLeftHeight = Node<T>::conditional_select(leftLeftNode->height, leftLeftHeight, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
        rightRightHeight = Node<T>::conditional_select(rightRightNode->height, rightRightHeight, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
        delete leftLeftNode;
        delete rightRightNode;

        //------------------------------------------------
        // Rotate
        //------------------------------------------------

        Node<T> *targetRotateNode = Node<T>::clone(tmpDummyNode);
        Node<T> *oppositeRotateNode = Node<T>::clone(tmpDummyNode);

        Node<T>::conditional_assign(targetRotateNode, leftNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(targetRotateNode, rightNode, !cond1 && !cond2 && !cond3 && cond4);

        Node<T>::conditional_assign(oppositeRotateNode, leftRightNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(oppositeRotateNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);

        int rotateHeight = 0;
        rotateHeight = Node<T>::conditional_select(leftLeftHeight, rotateHeight, !cond1 && !cond2 && cond3);
        rotateHeight = Node<T>::conditional_select(rightRightHeight, rotateHeight, !cond1 && !cond2 && !cond3 && cond4);

        bool isRightRotate = !cond1 && !cond2 && !cond3 && cond4;
        bool isDummyRotate = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));

        rotate(targetRotateNode, oppositeRotateNode, rotateHeight, isRightRotate, isDummyRotate);

        Node<T>::conditional_assign(leftNode, targetRotateNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(rightNode, targetRotateNode, !cond1 && !cond2 && !cond3 && cond4);

        delete targetRotateNode;

        Node<T>::conditional_assign(leftRightNode, oppositeRotateNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(rightLeftNode, oppositeRotateNode, !cond1 && !cond2 && !cond3 && cond4);

        delete oppositeRotateNode;

        unsigned long long newP = RandomPath();
        unsigned long long oldLeftRightPos = RandomPath();
        oldLeftRightPos = Node<T>::conditional_select(leftNode->pos, oldLeftRightPos, !cond1 && !cond2 && cond3);
        oldLeftRightPos = Node<T>::conditional_select(rightNode->pos, oldLeftRightPos, !cond1 && !cond2 && !cond3 && cond4);

        leftNode->pos = Node<T>::conditional_select(newP, leftNode->pos, !cond1 && !cond2 && cond3);
        rightNode->pos = Node<T>::conditional_select(newP, rightNode->pos, !cond1 && !cond2 && !cond3 && cond4);

        Bid firstRotateWriteKey = dummy;
        firstRotateWriteKey = Bid::conditional_select(leftNode->key, firstRotateWriteKey, !cond1 && !cond2 && cond3);
        firstRotateWriteKey = Bid::conditional_select(rightNode->key, firstRotateWriteKey, !cond1 && !cond2 && !cond3 && cond4);

        Node<T> *firstRotateWriteNode = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(firstRotateWriteNode, leftNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(firstRotateWriteNode, rightNode, !cond1 && !cond2 && !cond3 && cond4);

        isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));
        tmp = readWriteCacheNode(firstRotateWriteKey, firstRotateWriteNode, false, isDummy); // WRITE
        delete tmp;
        delete firstRotateWriteNode;

        leftRightNode->leftPos = Node<T>::conditional_select(newP, leftRightNode->leftPos, !cond1 && !cond2 && cond3);
        rightLeftNode->rightPos = Node<T>::conditional_select(newP, rightLeftNode->rightPos, !cond1 && !cond2 && !cond3 && cond4);

        node->leftID = Bid::conditional_select(leftRightNode->key, node->leftID, !cond1 && !cond2 && cond3);
        node->leftPos = Node<T>::conditional_select(leftRightNode->pos, node->leftPos, !cond1 && !cond2 && cond3);

        node->rightID = Bid::conditional_select(rightLeftNode->key, node->rightID, !cond1 && !cond2 && !cond3 && cond4);
        node->rightPos = Node<T>::conditional_select(rightLeftNode->pos, node->rightPos, !cond1 && !cond2 && !cond3 && cond4);

        //------------------------------------------------
        // Second Rotate
        //------------------------------------------------

        targetRotateNode = Node<T>::clone(tmpDummyNode);

        Node<T>::conditional_assign(targetRotateNode, node, cond1 || cond2 || cond3 || cond4);

        oppositeRotateNode = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(oppositeRotateNode, leftNode, cond1);
        Node<T>::conditional_assign(oppositeRotateNode, rightNode, !cond1 && cond2);
        Node<T>::conditional_assign(oppositeRotateNode, leftRightNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(oppositeRotateNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);

        rotateHeight = 0;
        rotateHeight = Node<T>::conditional_select(rightHeight, rotateHeight, cond1 || (!cond1 && !cond2 && cond3));
        rotateHeight = Node<T>::conditional_select(leftHeight, rotateHeight, (!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4));

        isRightRotate = cond1 || (!cond1 && !cond2 && cond3);
        isDummyRotate = !(cond1 || cond2 || cond3 || cond4);

        rotate(targetRotateNode, oppositeRotateNode, rotateHeight, isRightRotate, isDummyRotate);

        Node<T>::conditional_assign(node, targetRotateNode, cond1 || cond2 || cond3 || cond4);

        delete targetRotateNode;

        Node<T>::conditional_assign(leftNode, oppositeRotateNode, cond1);
        Node<T>::conditional_assign(rightNode, oppositeRotateNode, !cond1 && cond2);
        Node<T>::conditional_assign(leftRightNode, oppositeRotateNode, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(rightLeftNode, oppositeRotateNode, !cond1 && !cond2 && !cond3 && cond4);

        delete oppositeRotateNode;

        //------------------------------------------------
        // Last Two Writes
        //------------------------------------------------

        newP = RandomPath();

        Bid firstReadCache = dummy;
        firstReadCache = Bid::conditional_select(node->leftID, firstReadCache, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
        firstReadCache = Bid::conditional_select(node->rightID, firstReadCache, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

        isDummy = !(!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation);
        tmp = readWriteCacheNode(firstReadCache, tmpDummyNode, true, isDummy);

        Node<T>::conditional_assign(leftNode, tmp, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
        Node<T>::conditional_assign(rightNode, tmp, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

        delete tmp;

        Bid finalFirstWriteKey = dummy;
        finalFirstWriteKey = Bid::conditional_select(node->key, finalFirstWriteKey, cond1 || (!cond1 && cond2));
        finalFirstWriteKey = Bid::conditional_select(leftNode->key, finalFirstWriteKey, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft));
        finalFirstWriteKey = Bid::conditional_select(rightNode->key, finalFirstWriteKey, (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft));

        Node<T> *finalFirstWriteNode = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(finalFirstWriteNode, node, cond1 || (!cond1 && cond2));
        Node<T>::conditional_assign(finalFirstWriteNode, leftNode, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft));
        Node<T>::conditional_assign(finalFirstWriteNode, rightNode, (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft));

        unsigned long long finalFirstWritePos = dumyPos;
        finalFirstWritePos = Node<T>::conditional_select(node->pos, finalFirstWritePos, cond1 || (!cond1 && cond2));
        finalFirstWritePos = Node<T>::conditional_select(oldLeftRightPos, finalFirstWritePos, ((!cond1 && !cond2 && cond3)) || ((!cond1 && !cond2 && !cond3 && cond4)));
        finalFirstWritePos = Node<T>::conditional_select(leftNode->pos, finalFirstWritePos, (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft));
        finalFirstWritePos = Node<T>::conditional_select(rightNode->pos, finalFirstWritePos, (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft));

        unsigned long long finalFirstWriteNewPos = dumyPos;
        finalFirstWriteNewPos = Node<T>::conditional_select(newP, finalFirstWriteNewPos, cond1 || (!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation));
        finalFirstWriteNewPos = Node<T>::conditional_select(leftNode->pos, finalFirstWriteNewPos, (!cond1 && !cond2 && cond3));
        finalFirstWriteNewPos = Node<T>::conditional_select(rightNode->pos, finalFirstWriteNewPos, (!cond1 && !cond2 && !cond3 && cond4));

        isDummy = !cond1 && !cond2 && !cond3 && !cond4 && !doubleRotation;

        tmp = oram->ReadWrite(finalFirstWriteKey, finalFirstWriteNode, finalFirstWritePos, finalFirstWriteNewPos, false, isDummy, false); // WRITE

        delete tmp;
        delete finalFirstWriteNode;

        node->pos = Node<T>::conditional_select(newP, node->pos, cond1 || (!cond1 && cond2));
        leftNode->rightPos = Node<T>::conditional_select(newP, leftNode->rightPos, cond1);
        rightNode->leftPos = Node<T>::conditional_select(newP, rightNode->leftPos, !cond1 && cond2);
        leftNode->pos = Node<T>::conditional_select(newP, leftNode->pos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
        rightNode->pos = Node<T>::conditional_select(newP, rightNode->pos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);
        node->leftPos = Node<T>::conditional_select(newP, node->leftPos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
        node->rightPos = Node<T>::conditional_select(newP, node->rightPos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

        Bid finalFirstWriteCacheWriteKey = dummy;
        finalFirstWriteCacheWriteKey = Bid::conditional_select(node->key, finalFirstWriteCacheWriteKey, cond1 || (!cond1 && cond2));
        finalFirstWriteCacheWriteKey = Bid::conditional_select(node->leftID, finalFirstWriteCacheWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
        finalFirstWriteCacheWriteKey = Bid::conditional_select(node->rightID, finalFirstWriteCacheWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

        Node<T> *finalFirstWriteCacheWrite = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(finalFirstWriteCacheWrite, node, cond1 || (!cond1 && cond2));
        Node<T>::conditional_assign(finalFirstWriteCacheWrite, leftNode, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
        Node<T>::conditional_assign(finalFirstWriteCacheWrite, rightNode, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

        isDummy = (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && !doubleRotation);

        tmp = readWriteCacheNode(finalFirstWriteCacheWriteKey, finalFirstWriteCacheWrite, false, isDummy); // WRITE

        delete tmp;
        delete finalFirstWriteCacheWrite;

        doubleRotation = Node<T>::conditional_select(false, doubleRotation, doubleRotation && (!cond1 && !cond2 && !cond3 && !cond4));
        doubleRotation = Node<T>::conditional_select(true, doubleRotation, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));
        //------------------------------------------------
        // Last Write
        //------------------------------------------------

        newP = RandomPath();

        Bid finalWriteKey = dummy;
        finalWriteKey = Bid::conditional_select(leftNode->key, finalWriteKey, cond1);
        finalWriteKey = Bid::conditional_select(rightNode->key, finalWriteKey, !cond1 && cond2);
        finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && cond3);
        finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && !cond3 && cond4);
        finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

        unsigned long long finalWritePos = dumyPos;
        finalWritePos = Node<T>::conditional_select(leftNode->pos, finalWritePos, cond1);
        finalWritePos = Node<T>::conditional_select(rightNode->pos, finalWritePos, !cond1 && cond2);
        finalWritePos = Node<T>::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && cond3);
        finalWritePos = Node<T>::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && !cond3 && cond4);
        finalWritePos = Node<T>::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

        unsigned long long finalWriteNewPos = dumyPos;
        finalWriteNewPos = Node<T>::conditional_select(newP, finalWriteNewPos, cond1 || (!cond1 && cond2) || (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

        isDummy = !(cond1 || cond2 || cond3 || cond4 || cond5);

        Node<T> *finalWriteNode = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(finalWriteNode, leftNode, cond1);
        Node<T>::conditional_assign(finalWriteNode, rightNode, !cond1 && cond2);
        Node<T>::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && cond3);
        Node<T>::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && !cond3 && cond4);
        Node<T>::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

        tmp = oram->ReadWrite(finalWriteKey, finalWriteNode, finalWritePos, finalWriteNewPos, false, isDummy, false); // WRITE
        delete tmp;
        delete finalWriteNode;

        leftNode->pos = Node<T>::conditional_select(newP, leftNode->pos, cond1);
        rightNode->pos = Node<T>::conditional_select(newP, rightNode->pos, !cond1 && cond2);
        node->pos = Node<T>::conditional_select(newP, node->pos, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

        Bid finalCacheWriteKey = dummy;
        finalCacheWriteKey = Bid::conditional_select(leftNode->key, finalCacheWriteKey, cond1);
        finalCacheWriteKey = Bid::conditional_select(rightNode->key, finalCacheWriteKey, !cond1 && cond2);
        finalCacheWriteKey = Bid::conditional_select(node->key, finalCacheWriteKey, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

        Node<T> *finalCacheWriteNode = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(finalCacheWriteNode, leftNode, cond1);
        Node<T>::conditional_assign(finalCacheWriteNode, rightNode, !cond1 && cond2);
        Node<T>::conditional_assign(finalCacheWriteNode, node, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

        isDummy = !cond1 && !cond2 && !cond3 && !cond4 && !cond5;

        tmp = readWriteCacheNode(finalCacheWriteKey, finalCacheWriteNode, false, isDummy); // WRITE

        delete tmp;
        delete finalCacheWriteNode;

        leftRightNode->rightPos = Node<T>::conditional_select(newP, leftRightNode->rightPos, !cond1 && !cond2 && cond3);
        rightLeftNode->leftPos = Node<T>::conditional_select(newP, rightLeftNode->leftPos, !cond1 && !cond2 && !cond3 && cond4);

        finalCacheWriteKey = dummy;
        finalCacheWriteKey = Bid::conditional_select(leftRightNode->key, finalCacheWriteKey, (!cond1 && !cond2 && cond3));
        finalCacheWriteKey = Bid::conditional_select(rightLeftNode->key, finalCacheWriteKey, (!cond1 && !cond2 && !cond3 && cond4));

        finalCacheWriteNode = Node<T>::clone(tmpDummyNode);
        Node<T>::conditional_assign(finalCacheWriteNode, leftRightNode, (!cond1 && !cond2 && cond3));
        Node<T>::conditional_assign(finalCacheWriteNode, rightLeftNode, (!cond1 && !cond2 && !cond3 && cond4));

        isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));

        tmp = readWriteCacheNode(finalCacheWriteKey, finalCacheWriteNode, false, isDummy); // WRITE

        delete tmp;
        delete finalCacheWriteNode;

        rootPos = Node<T>::conditional_select(leftNode->pos, rootPos, cond1);
        rootPos = Node<T>::conditional_select(rightNode->pos, rootPos, !cond1 && cond2);
        rootPos = Node<T>::conditional_select(leftRightNode->pos, rootPos, !cond1 && !cond2 && cond3);
        rootPos = Node<T>::conditional_select(rightLeftNode->pos, rootPos, !cond1 && !cond2 && !cond3 && cond4);
        rootPos = Node<T>::conditional_select(node->pos, rootPos, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

        retKey = Bid::conditional_select(leftNode->key, retKey, cond1);
        retKey = Bid::conditional_select(rightNode->key, retKey, !cond1 && cond2);
        retKey = Bid::conditional_select(leftRightNode->key, retKey, !cond1 && !cond2 && cond3);
        retKey = Bid::conditional_select(rightLeftNode->key, retKey, !cond1 && !cond2 && !cond3 && cond4);
        retKey = Bid::conditional_select(node->key, retKey, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

        height = Node<T>::conditional_select(leftNode->height, height, cond1);
        height = Node<T>::conditional_select(rightNode->height, height, !cond1 && cond2);
        height = Node<T>::conditional_select(leftRightNode->height, height, !cond1 && !cond2 && cond3);
        height = Node<T>::conditional_select(rightLeftNode->height, height, !cond1 && !cond2 && !cond3 && cond4);

        if (totheight == 1)
        {
            isDummy = !doubleRotation;
            unsigned long long finalPos = RandomPath();
            Node<T> *finalNode = readWriteCacheNode(retKey, tmpDummyNode, true, isDummy);
            tmp = oram->ReadWrite(finalNode->key, finalNode, finalNode->pos, finalPos, false, isDummy, false); // WRITE
            rootPos = Node<T>::conditional_select(finalPos, rootPos, doubleRotation);
            height = Node<T>::conditional_select(finalNode->height, height, doubleRotation);
            delete tmp;
            delete finalNode;
        }

        if (logTime)
        {
            ocall_stop_timer(&t, totheight + 100000);
            times[0][times[0].size() - 1] += t;
        }

        delete tmpDummyNode;
        delete node;
        delete leftNode;
        delete rightNode;
        delete leftRightNode;
        delete rightLeftNode;
        return retKey;
    }

    Bid insert2(Bid rootKey, unsigned long long &rootPos, Bid omapKey, vector<byte_t> value, int &height, Bid lastID, bool isDummyIns)
    {
        totheight++;
        unsigned long long rndPos = RandomPath();
        double t;
        std::array<byte_t, sizeof(T)> tmpval;
        std::fill(tmpval.begin(), tmpval.end(), 0);
        std::copy(value.begin(), value.end(), tmpval.begin());
        Node<T> *tmpDummyNode = new Node<T>();
        tmpDummyNode->isDummy = true;
        Bid dummy;
        Bid retKey;
        unsigned long long dumyPos;
        bool remainerIsDummy = false;
        dummy.setValue(oram->nextDummyCounter++);

        if (isDummyIns && !CTeq(CTcmp(totheight, (int)((float)oram->depth * 1.44)), -1))
        {
            Bid resKey = lastID;
            if (!exist)
            {
                Node<T> *nnode = newNode(omapKey, value);
                nnode->pos = RandomPath();
                height = nnode->height;
                rootPos = nnode->pos;
                oram->ReadWrite(omapKey, nnode, nnode->pos, nnode->pos, false, false, false);
                readWriteCacheNode(omapKey, nnode, false, false);
                resKey = nnode->key;
            }
            else
            {
                unsigned long long newP = RandomPath();
                Node<T> *previousNode = readWriteCacheNode(omapKey, tmpDummyNode, true, false);
                oram->ReadWrite(omapKey, previousNode, previousNode->pos, newP, false, false, false);
                previousNode->pos = newP;
                readWriteCacheNode(omapKey, previousNode, false, false);
            }
            return resKey;
        }
        /* 1. Perform the normal BST rotation */

        if (!isDummyIns && !rootKey.isZero())
        {
            remainerIsDummy = false;
        }
        else
        {
            remainerIsDummy = true;
        }

        Node<T> *node = nullptr;
        if (remainerIsDummy)
        {
            node = oram->ReadWrite(dummy, tmpDummyNode, rootPos, rootPos, true, true, tmpval, Bid::CTeq(Bid::CTcmp(rootKey, omapKey), 0), true); // READ
            readWriteCacheNode(dummy, tmpDummyNode, false, true);
        }
        else
        {
            node = oram->ReadWrite(rootKey, tmpDummyNode, rootPos, rootPos, true, false, tmpval, Bid::CTeq(Bid::CTcmp(rootKey, omapKey), 0), true); // READ
            readWriteCacheNode(rootKey, node, false, false);
        }

        int balance = -1;
        int leftHeight = -1;
        int rightHeight = -1;
        Node<T> *leftNode = new Node<T>();
        bool leftNodeisNull = true;
        Node<T> *rightNode = new Node<T>();
        bool rightNodeisNull = true;
        std::array<byte_t, sizeof(T)> garbage;
        bool childDirisLeft = false;

        unsigned long long newRLPos = RandomPath();

        if (!remainerIsDummy)
        {
            if (omapKey < node->key)
            {
                node->leftID = insert(node->leftID, node->leftPos, omapKey, value, leftHeight, dummy, false);
                if (!node->rightID.isZero())
                {
                    Node<T> *rightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, node->rightPos, true, false, true); // READ
                    readWriteCacheNode(node->rightID, rightNode, false, false);
                    rightNodeisNull = false;
                    rightHeight = rightNode->height;
                    std::fill(garbage.begin(), garbage.end(), 0);
                    std::copy(value.begin(), value.end(), garbage.begin());
                    remainerIsDummy = false;
                }
                else
                {
                    Node<T> *dummyright = oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true, true);
                    readWriteCacheNode(dummy, dummyright, false, true);
                    rightHeight = 0;
                    std::fill(garbage.begin(), garbage.end(), 0);
                    std::copy(value.begin(), value.end(), garbage.begin());
                    remainerIsDummy = false;
                }
                childDirisLeft = true;
                totheight--;
            }
            else if (omapKey > node->key)
            {
                node->rightID = insert(node->rightID, node->rightPos, omapKey, value, rightHeight, dummy, false);
                if (!node->leftID.isZero())
                {
                    Node<T> *leftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, node->leftPos, true, false, true); // READ
                    readWriteCacheNode(node->leftID, leftNode, false, false);
                    leftNodeisNull = false;
                    leftHeight = leftNode->height;
                    std::fill(garbage.begin(), garbage.end(), 0);
                    std::copy(value.begin(), value.end(), garbage.begin());
                    remainerIsDummy = false;
                }
                else
                {
                    Node<T> *dummyleft = oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true, true);
                    readWriteCacheNode(dummy, dummyleft, false, true);
                    leftHeight = 0;
                    std::fill(garbage.begin(), garbage.end(), 0);
                    std::copy(value.begin(), value.end(), garbage.begin());
                    remainerIsDummy = false;
                }
                childDirisLeft = false;
                totheight--;
            }
            else
            {
                exist = true;
                Bid resValBid = insert(rootKey, rootPos, omapKey, value, height, node->key, true);
                Node<T> *dummyleft = oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true, true);
                totheight--;
                node = readWriteCacheNode(node->key, tmpDummyNode, true, false);
                rootPos = node->pos;
                height = node->height;
                retKey = node->key;
                remainerIsDummy = true;
            }
        }
        else
        {
            Bid resValBid = insert(rootKey, rootPos, omapKey, value, height, node->key, true);
            Node<T> *dummyleft = oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true, true);
            totheight--;
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            std::fill(garbage.begin(), garbage.end(), 0);
            std::copy(value.begin(), value.end(), garbage.begin());
            retKey = resValBid;
            remainerIsDummy = remainerIsDummy;
        }
        /* 2. Update height of this ancestor node */
        if (remainerIsDummy)
        {
            max(leftHeight, rightHeight) + 1;
            node->height = node->height;
            height = height;
        }
        else
        {
            node->height = max(leftHeight, rightHeight) + 1;
            height = node->height;
        }
        /* 3. Get the balance factor of this ancestor node to check whether this node became unbalanced */
        if (remainerIsDummy)
        {
            balance = balance;
        }
        else
        {
            balance = leftHeight - rightHeight;
        }

        ocall_start_timer(totheight + 100000);

        if (remainerIsDummy == false && CTeq(CTcmp(balance, 1), 1) && omapKey < node->leftID)
        {
            //                printf("log1\n");
            // Left Left Case
            if (leftNodeisNull)
            {
                delete leftNode;
                leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false); // READ
                leftNodeisNull = false;
                node->leftPos = leftNode->pos;
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true); // WRITE
            }

            //------------------------DUMMY---------------------------
            Node<T> *leftRightNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            leftRightNode->rightPos = leftRightNode->rightPos;

            int leftLeftHeight = 0;
            if (leftNode->leftID != 0)
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            else
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            rotate(tmpDummyNode, tmpDummyNode, leftLeftHeight, false, true);

            RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, false, true); // WRITE

            node->rightID = node->rightID;
            node->rightPos = node->rightPos;
            node->leftID = node->leftID;
            node->leftPos = node->leftPos;
            //---------------------------------------------------

            rotate(node, leftNode, rightHeight, true);

            unsigned long long newP = RandomPath();
            oram->ReadWrite(node->key, node, node->pos, newP, false, false, false); // WRITE
            node->pos = newP;
            leftNode->rightPos = newP;
            readWriteCacheNode(node->key, node, false, false); // WRITE

            newP = RandomPath();
            oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, newP, false, false, false); // WRITE
            leftNode->pos = newP;
            readWriteCacheNode(leftNode->key, leftNode, false, false); // WRITE
            readWriteCacheNode(dummy, tmpDummyNode, true, true);

            rootPos = leftNode->pos;
            height = leftNode->height;
            retKey = leftNode->key;
        }
        else if (remainerIsDummy == false && CTeq(CTcmp(balance, -1), -1) && omapKey > node->rightID)
        {
            //                printf("log2\n");
            // Right Right Case
            if (rightNodeisNull)
            {
                delete rightNode;
                rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false); // READ
                rightNodeisNull = false;
                node->rightPos = rightNode->pos;
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
            }

            //------------------------DUMMY---------------------------
            Node<T> *leftRightNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);

            int leftLeftHeight = 0;
            if (rightNode->leftID != 0)
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            else
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            rotate(tmpDummyNode, tmpDummyNode, leftLeftHeight, false, true);

            RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, false, true); // WRITE

            node->rightID = node->rightID;
            node->rightPos = node->rightPos;
            node->leftID = node->leftID;
            node->leftPos = node->leftPos;
            //---------------------------------------------------

            rotate(node, rightNode, leftHeight, false);

            unsigned long long newP = RandomPath();
            oram->ReadWrite(node->key, node, node->pos, newP, false, false, false); // WRITE
            node->pos = newP;
            readWriteCacheNode(node->key, node, false, false); // WRITE

            rightNode->leftPos = newP;
            newP = RandomPath();
            oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, newP, false, false, false); // WRITE
            rightNode->pos = newP;
            readWriteCacheNode(rightNode->key, rightNode, false, false); // WRITE
            readWriteCacheNode(dummy, tmpDummyNode, true, true);

            rootPos = rightNode->pos;
            height = rightNode->height;
            retKey = rightNode->key;
        }
        else if (remainerIsDummy == false && CTeq(CTcmp(balance, 1), 1) && omapKey > node->leftID)
        {
            //                printf("log3\n");
            // Left Right Case
            if (leftNodeisNull)
            {
                delete leftNode;
                leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false); // READ
                leftNodeisNull = false;
                node->leftPos = leftNode->pos;
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
            }

            Node<T> *leftRightNode = readWriteCacheNode(leftNode->rightID, tmpDummyNode, true, false); // READ

            int leftLeftHeight = 0;
            if (leftNode->leftID != 0)
            {
                Node<T> *leftLeftNode = readWriteCacheNode(leftNode->leftID, tmpDummyNode, true, false); // READ
                leftLeftHeight = leftLeftNode->height;
                delete leftLeftNode;
            }
            else
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }

            rotate(leftNode, leftRightNode, leftLeftHeight, false);

            unsigned long long newP = RandomPath();
            unsigned long long oldLeftRightPos = leftNode->pos;
            leftNode->pos = newP;
            readWriteCacheNode(leftNode->key, leftNode, false, false); // WRITE
            leftRightNode->leftPos = newP;

            node->leftID = leftRightNode->key;
            node->leftPos = leftRightNode->pos;

            rotate(node, leftRightNode, rightHeight, true);

            oram->ReadWrite(leftNode->key, leftNode, oldLeftRightPos, leftNode->pos, false, false, false); // WRITE
            newP = RandomPath();
            oram->ReadWrite(node->key, node, node->pos, newP, false, false, false); // WRITE
            node->pos = newP;
            readWriteCacheNode(node->key, node, false, false); // WRITE
            leftRightNode->rightPos = newP;
            readWriteCacheNode(leftRightNode->key, leftRightNode, false, false); // WRITE
            doubleRotation = true;
            readWriteCacheNode(dummy, tmpDummyNode, true, true);

            rootPos = leftRightNode->pos;
            height = leftRightNode->height;
            retKey = leftRightNode->key;
        }
        else if (remainerIsDummy == false && CTeq(CTcmp(balance, -1), -1) && omapKey < node->rightID)
        {
            //                printf("log4\n");
            //  Right-Left Case
            if (rightNodeisNull)
            {
                delete rightNode;
                rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false); // READ
                rightNodeisNull = false;
                node->rightPos = rightNode->pos;
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->rightPos = node->rightPos;
            }

            Node<T> *rightLeftNode = readWriteCacheNode(rightNode->leftID, tmpDummyNode, true, false); // READ

            int rightRightHeight = 0;
            if (rightNode->rightID != 0)
            {
                Node<T> *rightRightNode = readWriteCacheNode(rightNode->rightID, tmpDummyNode, true, false); // READ
                rightRightHeight = rightRightNode->height;
                delete rightRightNode;
            }
            else
            {
                Node<T> *rightRightNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                rightRightHeight = rightRightHeight;
                delete rightRightNode;
            }

            rotate(rightNode, rightLeftNode, rightRightHeight, true);

            unsigned long long newP = RandomPath();
            unsigned long long oldLeftRightPos = rightNode->pos;
            rightNode->pos = newP;
            readWriteCacheNode(rightNode->key, rightNode, false, false); // WRITE
            rightLeftNode->rightPos = newP;

            node->rightID = rightLeftNode->key;
            node->rightPos = rightLeftNode->pos;

            rotate(node, rightLeftNode, leftHeight, false);

            oram->ReadWrite(rightNode->key, rightNode, oldLeftRightPos, rightNode->pos, false, false, false); // WRITE
            newP = RandomPath();
            oram->ReadWrite(node->key, node, node->pos, newP, false, false, false); // WRITE
            node->pos = newP;
            readWriteCacheNode(node->key, node, false, false); // WRITE
            rightLeftNode->leftPos = newP;
            readWriteCacheNode(rightLeftNode->key, rightLeftNode, false, false); // WRITE
            doubleRotation = true;
            readWriteCacheNode(dummy, tmpDummyNode, true, true);

            rootPos = rightLeftNode->pos;
            height = rightLeftNode->height;
            retKey = rightLeftNode->key;
        }
        else if (remainerIsDummy == false)
        {
            //                printf("log5\n");
            //------------------------DUMMY---------------------------
            if (node == NULL)
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->rightPos = node->rightPos;
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->rightPos = node->rightPos;
            }

            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            node->rightPos = node->rightPos;

            int leftLeftHeight = 0;
            if (node->leftID != 0)
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->leftPos = node->leftPos;
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            else
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->leftPos = node->leftPos;
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }

            rotate(tmpDummyNode, tmpDummyNode, 0, false, true);

            RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, false, true); // WRITE

            node->rightID = node->rightID;
            node->rightPos = node->rightPos;
            node->leftID = node->leftID;
            node->leftPos = node->leftPos;

            rotate(tmpDummyNode, tmpDummyNode, 0, false, true);

            unsigned long long newP = RandomPath();
            if (doubleRotation)
            {
                doubleRotation = false;
                if (childDirisLeft)
                {
                    leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false);
                    oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, newP, false, false, false); // WRITE
                    leftNode->pos = newP;
                    node->leftPos = newP;
                    Node<T> *t = readWriteCacheNode(node->leftID, leftNode, false, false);
                    delete t;
                }
                else
                {
                    rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false);
                    oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, newP, false, false, false); // WRITE
                    rightNode->pos = newP;
                    node->rightPos = newP;
                    Node<T> *t = readWriteCacheNode(node->rightID, rightNode, false, false);
                    delete t;
                }
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                oram->ReadWrite(dummy, tmpDummyNode, rightNode->pos, rightNode->pos, false, true, false); // WRITE
                Node<T> *t = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                delete t;
            }
            //----------------------------------------------------------------------------------------

            newP = RandomPath();
            oram->ReadWrite(node->key, node, node->pos, newP, false, false, false); // WRITE
            node->pos = newP;
            readWriteCacheNode(node->key, node, false, false); // WRITE
            readWriteCacheNode(dummy, tmpDummyNode, true, true);

            rootPos = node->pos;
            height = height;
            retKey = node->key;
        }
        else
        {
            // printf("log6\n");
            //------------------------DUMMY---------------------------
            if (node == NULL)
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->rightPos = node->rightPos;
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->rightPos = node->rightPos;
            }

            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            int leftLeftHeight = 0;
            if (node->leftID != 0)
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->leftPos = node->leftPos;
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            else
            {
                Node<T> *leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                node->leftPos = node->leftPos;
                leftLeftHeight = leftLeftHeight;
                delete leftLeftNode;
            }
            rotate(tmpDummyNode, tmpDummyNode, 0, false, true);

            RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, false, true); // WRITE

            node->rightID = node->rightID;
            node->rightPos = node->rightPos;
            node->leftID = node->leftID;
            node->leftPos = node->leftPos;

            rotate(tmpDummyNode, tmpDummyNode, 0, false, true);

            unsigned long long newP = RandomPath();
            if (doubleRotation)
            {
                doubleRotation = false;
                if (childDirisLeft)
                {
                    leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false);
                    oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, newP, false, false, false); // WRITE
                    leftNode->pos = newP;
                    node->leftPos = newP;
                    Node<T> *t = readWriteCacheNode(node->leftID, leftNode, false, false);
                    delete t;
                }
                else
                {
                    rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false);
                    oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, newP, false, false, false); // WRITE
                    rightNode->pos = newP;
                    node->rightPos = newP;
                    Node<T> *t = readWriteCacheNode(node->rightID, rightNode, false, false);
                    delete t;
                }
            }
            else
            {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                oram->ReadWrite(dummy, tmpDummyNode, rightNode->pos, rightNode->pos, false, true, false); // WRITE
                Node<T> *t = readWriteCacheNode(dummy, tmpDummyNode, true, true);
                delete t;
            }

            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true, false); // WRITE
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            readWriteCacheNode(dummy, tmpDummyNode, true, true);

            rootPos = rootPos;
            height = height;
            retKey = retKey;
            //----------------------------------------------------------------------------------------
        }
        ocall_stop_timer(&t, totheight + 100000);
        //    times[0][times[0].size() - 1] += t;
        delete tmpDummyNode;
        return retKey;
    }

    Bid searchInsert(Bid rootKey, unsigned long long &pos, Bid key, string &value, int &height, Bid lastID, bool isDummyIns = false);

    vector<byte_t> search(Node<T> *rootNode, Bid omapKey)
    {
        Bid curKey = rootNode->key;
        unsigned long long lastPos = rootNode->pos;
        unsigned long long newPos = RandomPath();
        rootNode->pos = newPos;
        vector<byte_t> res(sizeof(T), 0);
        // string res(16, '\0');
        Bid dumyID = oram->nextDummyCounter;
        Node<T> *tmpDummyNode = new Node<T>();
        tmpDummyNode->isDummy = true;
        std::array<byte_t, sizeof(T)> resVec;
        Node<T> *head;
        int dummyState = 0;
        int upperBound = (int)(1.44 * oram->depth);
        bool found = false;
        unsigned long long dumyPos;

        do
        {
            unsigned long long rnd = RandomPath();
            unsigned long long rnd2 = RandomPath();
            bool isDummyAction = Node<T>::CTeq(Node<T>::CTcmp(dummyState, 1), 0);
            head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey);

            bool cond1 = Node<T>::CTeq(Node<T>::CTcmp(dummyState, 1), 0);
            bool cond2 = Node<T>::CTeq(Bid::CTcmp(head->key, omapKey), 1);
            bool cond3 = Node<T>::CTeq(Bid::CTcmp(head->key, omapKey), -1);
            bool cond4 = Node<T>::CTeq(Bid::CTcmp(head->key, omapKey), 0);

            lastPos = Node<T>::conditional_select(rnd, lastPos, cond1);
            lastPos = Node<T>::conditional_select(head->leftPos, lastPos, !cond1 && cond2);
            lastPos = Node<T>::conditional_select(head->rightPos, lastPos, !cond1 && !cond2 && cond3);

            curKey = dumyID;

            bool leftIsZero = head->leftID.isZero();
            bool rightIsZero = head->rightID.isZero();
            for (int k = 0; k < curKey.id.size(); k++)
            {
                curKey.id[k] = Node<T>::conditional_select(head->leftID.id[k], curKey.id[k], !cond1 && cond2 && !leftIsZero);
                curKey.id[k] = Node<T>::conditional_select(head->rightID.id[k], curKey.id[k], !cond1 && !cond2 && cond3 && !rightIsZero);
            }

            newPos = Node<T>::conditional_select(rnd, newPos, cond1);
            newPos = Node<T>::conditional_select(rnd2, newPos, !cond1 && cond2);
            newPos = Node<T>::conditional_select(rnd2, newPos, !cond1 && !cond2 && cond3);

            for (int i = 0; i < resVec.size(); i++)
            {
                resVec[i] = Bid::conditional_select(head->value[i], resVec[i], !cond1);
            }

            dummyState = Node<T>::conditional_select(1, dummyState, !cond1 && !cond2 && !cond3 && cond4);
            dummyState = Node<T>::conditional_select(1, dummyState, !cond4 && ((!cond1 && cond2 && leftIsZero) || (!cond1 && !cond2 && cond3 && rightIsZero)));
            found = Node<T>::conditional_select(true, found, !cond1 && !cond2 && !cond3 && cond4);
            delete head;
        } while (oram->readCnt <= upperBound);
        delete tmpDummyNode;
        for (int i = 0; i < resVec.size(); i++)
        {
            res[i] = Bid::conditional_select(resVec[i], (byte_t)0, found);
        }
        //    res.assign(resVec.begin(), resVec.end());
        return res;
    }

    void batchSearch(Node<T> *head, vector<Bid> keys, vector<Node<T> *> *results)
    {
        //    if (head == NULL || head->key == 0) {
        //        return;
        //    }
        //    head = oram->ReadNode(head->key, head->pos, head->pos);
        //    bool getLeft = false, getRight = false;
        //    vector<Bid> leftkeys, rightkeys;
        //    for (Bid bid : keys) {
        //        if (head->key > bid) {
        //            getLeft = true;
        //            leftkeys.push_back(bid);
        //        }
        //        if (head->key < bid) {
        //            getRight = true;
        //            rightkeys.push_back(bid);
        //        }
        //        if (head->key == bid) {
        //            results->push_back(head);
        //        }
        //    }
        //    if (getLeft) {
        //        batchSearch(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), leftkeys, results);
        //    }
        //    if (getRight) {
        //        batchSearch(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), rightkeys, results);
        //    }
    }

    void printTree(Node<T> *rt, int indent)
    {
        if (rt != 0 && rt->key != 0)
        {
            Node<T> *tmpDummyNode = new Node<T>();
            tmpDummyNode->isDummy = true;
            Node<T> *root = oram->ReadWriteTest(rt->key, tmpDummyNode, rt->pos, rt->pos, true, false, true);

            if (root->leftID != 0)
                printTree(oram->ReadWriteTest(root->leftID, tmpDummyNode, root->leftPos, root->leftPos, true, false, true), indent + 4);
            if (indent > 0)
            {
                for (int i = 0; i < indent; i++)
                {
                    printf(" ");
                }
            }

            string value;
            value.assign(root->value.begin(), root->value.end());
            printf("Key:%lld Height:%lld Pos:%lld LeftID:%lld LeftPos:%lld RightID:%lld RightPos:%lld\n", root->key.getValue(), root->height, root->pos, root->leftID.getValue(), root->leftPos, root->rightID.getValue(), root->rightPos);
            if (root->rightID != 0)
                printTree(oram->ReadWriteTest(root->rightID, tmpDummyNode, root->rightPos, root->rightPos, true, false, true), indent + 4);
            delete tmpDummyNode;
            delete rt;
            delete root;
        }
    }

    void startOperation(bool batchWrite = false)
    {
        oram->start(batchWrite);
        totheight = 0;
        exist = false;
        doubleRotation = false;
    }

    void setupInsert(Bid &rootKey, int &rootPos, map<Bid, string> &pairs);

    void finishOperation()
    {
        for (auto item : avlCache)
        {
            delete item;
        }
        avlCache.clear();
        oram->finilize();
    }
};

#endif /* AVLTREE_H */
