/*
** Copyright (c) 2026 LunarG, Inc.
** Copyright (c) 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
*/

#ifndef GFXRECONSTRUCT_UTIL_INTERVAL_TREE_H
#define GFXRECONSTRUCT_UTIL_INTERVAL_TREE_H

#include "util/defines.h"
#include "util/logging.h"

#include <algorithm>
#include <memory>
#include <utility>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(util)

/**
 * @brief interval_tree is a height-balanced binary search tree storing half-open intervals.
 *
 * Each node records the greatest upper bound in its subtree. This augmentation
 * allows an overlap query to discard subtrees which cannot contain a match.
 * The tree is AVL-balanced, so insertion, erasure, and intersection are
 * O(log n).
 *
 * Intervals are half-open: [a, b) intersects [c, d) when a < d and c < b.
 * Inserting an interval already present in the tree has no effect. Pointers
 * returned by intersection remain valid until a non-const operation is
 * performed on the tree. The pointed-to interval must not be modified.
 *
 * @tparam T
 */
template <typename T>
class interval_tree
{
  public:
    using interval_type = std::pair<T, T>;

    interval_tree() = default;

    interval_tree(const interval_tree& other) : root_(clone(other.root_)) {}

    interval_tree& operator=(const interval_tree& other)
    {
        if (this != &other)
        {
            root_ = clone(other.root_);
        }
        return *this;
    }

    interval_tree(interval_tree&&) noexcept            = default;
    interval_tree& operator=(interval_tree&&) noexcept = default;

    void insert(interval_type interval)
    {
        GFXRECON_ASSERT(is_valid(interval));
        if (is_valid(interval))
        {
            root_ = insert(std::move(root_), std::move(interval));
        }
    }

    void erase(const interval_type& interval)
    {
        GFXRECON_ASSERT(is_valid(interval));
        if (is_valid(interval))
        {
            root_ = erase(std::move(root_), interval);
        }
    }

    interval_type* intersection(const interval_type& interval)
    {
        return const_cast<interval_type*>(static_cast<const interval_tree&>(*this).intersection(interval));
    }

    const interval_type* intersection(const interval_type& interval) const
    {
        GFXRECON_ASSERT(is_valid(interval));
        if (!is_valid(interval))
        {
            return nullptr;
        }

        const Node* node = root_.get();
        while (node != nullptr)
        {
            if (overlaps(node->interval, interval))
            {
                return &node->interval;
            }

            if ((node->left != nullptr) && less(interval.first, node->left->maximum))
            {
                node = node->left.get();
            }
            else
            {
                node = node->right.get();
            }
        }

        return nullptr;
    }

    void clear() { root_.reset(); }

  private:
    struct Node
    {
        explicit Node(interval_type value) : interval(std::move(value)), maximum(interval.second) {}

        interval_type         interval;
        T                     maximum;
        int                   height{ 1 };
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    bool less(const T& lhs, const T& rhs) const { return lhs < rhs; }

    bool is_valid(const interval_type& interval) const { return less(interval.first, interval.second); }

    bool interval_less(const interval_type& lhs, const interval_type& rhs) const
    {
        return less(lhs.first, rhs.first) || (!less(rhs.first, lhs.first) && less(lhs.second, rhs.second));
    }

    bool overlaps(const interval_type& lhs, const interval_type& rhs) const
    {
        return less(lhs.first, rhs.second) && less(rhs.first, lhs.second);
    }

    int height(const std::unique_ptr<Node>& node) const { return node != nullptr ? node->height : 0; }

    int balance_factor(const Node& node) const { return height(node.left) - height(node.right); }

    void update(Node& node)
    {
        node.maximum = node.interval.second;
        if ((node.left != nullptr) && less(node.maximum, node.left->maximum))
        {
            node.maximum = node.left->maximum;
        }
        if ((node.right != nullptr) && less(node.maximum, node.right->maximum))
        {
            node.maximum = node.right->maximum;
        }
        node.height = 1 + std::max(height(node.left), height(node.right));
    }

    std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> node)
    {
        GFXRECON_ASSERT(node != nullptr && node->right != nullptr);

        auto new_root = std::move(node->right);
        node->right   = std::move(new_root->left);
        update(*node);
        new_root->left = std::move(node);
        update(*new_root);
        return new_root;
    }

    std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> node)
    {
        GFXRECON_ASSERT(node != nullptr && node->left != nullptr);

        auto new_root = std::move(node->left);
        node->left    = std::move(new_root->right);
        update(*node);
        new_root->right = std::move(node);
        update(*new_root);
        return new_root;
    }

    std::unique_ptr<Node> rebalance(std::unique_ptr<Node> node)
    {
        GFXRECON_ASSERT(node != nullptr);
        update(*node);

        const int balance = balance_factor(*node);
        if (balance > 1)
        {
            if (balance_factor(*node->left) < 0)
            {
                node->left = rotate_left(std::move(node->left));
            }
            return rotate_right(std::move(node));
        }
        if (balance < -1)
        {
            if (balance_factor(*node->right) > 0)
            {
                node->right = rotate_right(std::move(node->right));
            }
            return rotate_left(std::move(node));
        }
        return node;
    }

    std::unique_ptr<Node> insert(std::unique_ptr<Node> node, interval_type interval)
    {
        if (node == nullptr)
        {
            return std::make_unique<Node>(std::move(interval));
        }

        if (interval_less(interval, node->interval))
        {
            node->left = insert(std::move(node->left), std::move(interval));
        }
        else if (interval_less(node->interval, interval))
        {
            node->right = insert(std::move(node->right), std::move(interval));
        }
        else
        {
            return node;
        }
        return rebalance(std::move(node));
    }

    std::unique_ptr<Node> erase(std::unique_ptr<Node> node, const interval_type& interval)
    {
        if (node == nullptr)
        {
            return nullptr;
        }

        if (interval_less(interval, node->interval))
        {
            node->left = erase(std::move(node->left), interval);
        }
        else if (interval_less(node->interval, interval))
        {
            node->right = erase(std::move(node->right), interval);
        }
        else
        {
            if (node->left == nullptr)
            {
                return std::move(node->right);
            }
            if (node->right == nullptr)
            {
                return std::move(node->left);
            }

            Node* successor = node->right.get();
            while (successor->left != nullptr)
            {
                successor = successor->left.get();
            }
            node->interval = successor->interval;
            node->right    = erase(std::move(node->right), successor->interval);
        }
        return rebalance(std::move(node));
    }

    std::unique_ptr<Node> clone(const std::unique_ptr<Node>& node)
    {
        if (node == nullptr)
        {
            return nullptr;
        }

        auto result     = std::make_unique<Node>(node->interval);
        result->maximum = node->maximum;
        result->height  = node->height;
        result->left    = clone(node->left);
        result->right   = clone(node->right);
        return result;
    }

    std::unique_ptr<Node> root_;
};

GFXRECON_END_NAMESPACE(util)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECONSTRUCT_UTIL_INTERVAL_TREE_H
