#if 0
#######################################################################################
# The MIT License

# Copyright (c) 2014       Hannes Schulz, University of Bonn  <schulz@ais.uni-bonn.de>
# Copyright (c) 2013       Benedikt Waldvogel, University of Bonn <mail@bwaldvogel.de>
# Copyright (c) 2008-2009  Sebastian Nowozin                       <nowozin@gmail.com>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#######################################################################################
#endif
#ifndef CURFIL_RANDOM_TREE_IMAGE_GPU_H
#define CURFIL_RANDOM_TREE_IMAGE_GPU_H

#include <cuda_runtime_api.h>
#include <limits.h>
#include <map>
#include <set>
#include <vector_types.h>
#include <vector>

#include "image.h"
#include "random_tree_image.h"

namespace curfil {

static const int colorChannels = 3;
static const int depthChannels = 2;

static const int depthChannel = 0;
static const int depthValidChannel = 1;

static const unsigned int NODES_PER_TREE_LAYER = 2048;
static const unsigned int LAYERS_PER_TREE = 16;

/**
 * Helper class to map random forest data to texture cache on GPU.
 * @ingroup LRU_cache
 */
class TreeNodes {

public:

    /**
     * Prepare the random forest data from the given nodes such that it can be transferred to the GPU.
     */
    TreeNodes(const TreeNodes& other);

    /**
     * Prepare the random forest data from the given tree such that it can be transferred to the GPU.
     */
    TreeNodes(const boost::shared_ptr<const RandomTree<PixelInstance, ImageFeatureFunction> >& tree);

    /**
     * @return The of the tree in random forest.
     */
    size_t getTreeId() const {
        return m_treeId;
    }

    /**
     * @return The total number of nodes in the tree.
     */
    size_t numNodes() const {
        return m_numNodes;
    }

    /**
     * @return the number of labels (classes) that existed in the training dataset.
     */
    size_t numLabels() const {
        return m_numLabels;
    }

    /**
     * @return the per-node size in bytes
     */
    size_t sizePerNode() const {
        return m_sizePerNode;
    }

    /**
     * @return a reference to the n-d array of the internal data structure that can be transferred to the GPU.
     */
    cuv::ndarray<int8_t, cuv::host_memory_space>& data() {
        return m_data;
    }

    /**
     * @return a reference to the n-d array of the internal data structure that can be transferred to the GPU. - const
     */
    const cuv::ndarray<int8_t, cuv::host_memory_space>& data() const {
        return m_data;
    }

private:

    static const size_t offsetLeftNode = 0;
    static const size_t offsetTypes = 4;
    static const size_t offsetFeatures = offsetTypes + 4;
    static const size_t offsetChannels = offsetFeatures + 8;
    static const size_t offsetThreshold = offsetChannels + 4;
    static const size_t offsetHistograms = offsetThreshold + 4;

    size_t m_treeId;
    size_t m_numNodes;
    size_t m_numLabels;
    size_t m_sizePerNode;
    cuv::ndarray<int8_t, cuv::host_memory_space> m_data;

    template<class T>
    void setValue(size_t node, size_t offset, const T& value);

    void setLeftNodeOffset(size_t node, int offset);
    void setThreshold(size_t node, float threshold);
    void setHistogramValue(size_t node, size_t label, float value);
    void setType(size_t node, int8_t value);
    void setOffset1X(size_t node, int8_t value);
    void setOffset1Y(size_t node, int8_t value);
    void setRegion1X(size_t node, int8_t value);
    void setRegion1Y(size_t node, int8_t value);
    void setOffset2X(size_t node, int8_t value);
    void setOffset2Y(size_t node, int8_t value);
    void setRegion2X(size_t node, int8_t value);
    void setRegion2Y(size_t node, int8_t value);
    void setChannel1(size_t node, uint16_t value);
    void setChannel2(size_t node, uint16_t value);

    void convert(const boost::shared_ptr<const RandomTree<PixelInstance, ImageFeatureFunction> >& tree);

    TreeNodes& operator=(const TreeNodes& other);

};

/**
 * Abstract base class that implements a simple LRU cache on GPU.
 * @ingroup LRU_cache
 */
class DeviceCache {

public:

    virtual ~DeviceCache();

    /**
     * @return a mapping of the in-cache elements to their according positions
     */
    std::map<const void*, size_t>& getIdMap() {
        return elementIdMap;
    }

    /**
     * @return true if and only if the given element is in cache.
     */
    bool containsElement(const void* element) const;

    /**
     * @return the position in cache of the given element
     * @throws runtime_exception if the given element is not in cache
     */
    size_t getElementPos(const void* element) const;

    /**
     * Clear the entire cache.
     */
    void clear();

    /**
     * @return the total elapsed time in microseconds that was spent to transfer data to the cache
     */
    size_t getTotalTransferTimeMircoseconds() const {
        return totalTransferTimeMicroseconds;
    }

private:
    DeviceCache(const DeviceCache& other);
    DeviceCache& operator=(const DeviceCache& other);

protected:

    DeviceCache() :
            cacheSize(0), elementIdMap(), elementTimes(), currentTime(0), bound(false), totalTransferTimeMicroseconds(0) {
    }

    /**
     * @return whether texture is bound
     */
    bool isBound() const {
        return bound;
    }

    /**
     * set the texture bound property
     */
    void setBound(bool bound);

    /**
     * set the cache size to the passed size
     */
    void updateCacheSize(size_t cacheSize);

    virtual void bind() = 0; /**< bind texture to array */
    virtual void unbind() = 0; /**< unbind the texture */

    virtual void allocArray() = 0; /**< allocate a CUDA array */
    virtual void freeArray() = 0;  /**< clear texture data and free the array */

    /**
     * @return the cache size
     */
    size_t getCacheSize() const {
        return cacheSize;
    }

    /**
     * resize the cache and transfer the elements to GPU
     */
    void copyElements(size_t cacheSize, const std::set<const void*>& elements);

    /**
     * transfer the element's value at the passed position to GPU
     */
    virtual void transferElement(size_t pos, const void* element, cudaStream_t stream) = 0;

    // for logging
    virtual std::string getElementName(const void* element) const = 0; /**< @return the passed element's name */
    virtual std::string getElementsName() const = 0; /**< @return all elements names */

private:

    size_t cacheSize;
    std::map<const void*, size_t> elementIdMap;
    std::map<size_t, size_t> elementTimes;

    // poor man’s vector clock
    size_t currentTime;

    bool bound;

    size_t totalTransferTimeMicroseconds;

};

/**
 * A simple LRU cache of RGB-D images on GPU
 * @ingroup LRU_cache
 */
class ImageCache: public DeviceCache {

public:

    ImageCache();

    virtual ~ImageCache();

    /**
     * Resize the cache and explicitly transfer the given images to the GPU.
     */
    void copyImages(size_t imageCacheSize, const std::set<const RGBDImage*>& images);

    /**
     * Collect the images for the given samples, resize the cache and transfer the collected images to the GPU.
     */
    void copyImages(size_t imageCacheSize, const std::vector<const PixelInstance*>& samples);

private:
    ImageCache(const ImageCache& other);
    ImageCache& operator=(const ImageCache& other);

protected:

    virtual void bind();
    virtual void unbind();

    virtual void allocArray();
    virtual void freeArray();

    virtual void transferElement(size_t pos, const void* element, cudaStream_t stream);
    virtual std::string getElementName(const void* element) const;
    virtual std::string getElementsName() const;

private:

    int width;
    int height;

    cudaArray* colorTextureData;
    cudaArray* depthTextureData;

};

/**
 * A simple LRU cache of trees on GPU
 * @ingroup LRU_cache
 */
class TreeCache: public DeviceCache {

public:

    TreeCache();

    virtual ~TreeCache();

    /**
     * Resize the cache and transfer the given tree to the GPU.
     */
    void copyTree(size_t cacheSize, const TreeNodes* tree);

    /**
     * Resize the cache and transfer the given trees to the GPU.
     */
    void copyTrees(size_t cacheSize, const std::set<const TreeNodes*>& trees);

private:
    TreeCache(const TreeCache& other);
    TreeCache& operator=(const TreeCache& other);

protected:

    virtual void transferElement(size_t elementPos, const void* element, cudaStream_t stream);
    virtual std::string getElementName(const void* element) const;
    virtual std::string getElementsName() const;

    virtual void bind();
    virtual void unbind();

    virtual void freeArray();
    virtual void allocArray();

private:

    size_t sizePerNode;
    LabelType numLabels;

    cudaArray* treeTextureData;
};

class RandomTreeImage;

/**
 * Helper class for the unit test
 * @ingroup unit_testing
 */
class TreeNodeData {

public:
    int leftNodeOffset;	 /**< offset of its left node */
    int type;            /**< type of the feature */
    int8_t offset1X;     /**< x offset of the first region */
    int8_t offset1Y;     /**< y offset of the first region */
    int8_t region1X;     /**< width of the first region */
    int8_t region1Y;     /**< height of the first region */
    int8_t offset2X;     /**< x offset of the second region */
    int8_t offset2Y;     /**< y offset of the second region */
    int8_t region2X;     /**< width of the second region */
    int8_t region2Y;     /**< height of the second region */
    uint8_t channel1;    /**< first channel of the feature */
    uint8_t channel2;    /**< second channel of the feature */
    float threshold;     /**< threshold that the feature response is compared against */
    cuv::ndarray<float, cuv::host_memory_space> histogram; /**< histogram of the node */
};

/**
 * Public for the unit test
 */
TreeNodeData getTreeNode(const int nodeNr, const boost::shared_ptr<const TreeNodes>& treeData);

/**
 * Convert a random forest for RGB-D images to the TreeNodes structure that can be used to be transferred to the GPU.
 * Not intended to be called by a client.
 */
boost::shared_ptr<const TreeNodes> convertTree(const boost::shared_ptr<const RandomTreeImage>& randomTreeImage);

/**
 * Helper function to normalize an array of probabilities on GPU.
 * Not intended to be called by a client.
 */
void normalizeProbabilities(cuv::ndarray<float, cuv::dev_memory_space>& probabilities);

/**
 * Helper function to determine the per-row label with maximal probability, on GPU.
 * Not intended to be called by a client.
 */
void determineMaxProbabilities(const cuv::ndarray<float, cuv::dev_memory_space>& probabilities,
        cuv::ndarray<LabelType, cuv::dev_memory_space>& output);

/**
 * Helper function to classify (predict) the given RGB-D image on GPU.
 * Not intended to be called by a client.
 */
void classifyImage(int treeCacheSize, cuv::ndarray<float, cuv::dev_memory_space>& output, const RGBDImage& image,
        LabelType numLabels, const boost::shared_ptr<const TreeNodes>& treeData, bool useDepthImages = true);


// for the unit test
void clearImageCache();

}

#endif
