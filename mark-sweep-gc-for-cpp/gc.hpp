#ifndef MARK_SWEEP_GC_HPP
#define MARK_SWEEP_GC_HPP

#include <map>
#include <functional>      

#include <setjmp.h>
#include <string.h>

// Mark & Sweep 

namespace mark_sweep_gc {

    enum mark {
        GC_UNMARK = 0x0,
        GC_MARK = 0x1,
    };

    // Save the meta of memory
    class allocation {
    public:
        size_t  size_;
        void*   mem_;
        uint8_t target_;
        std::function<void()> destructor_;

        allocation(void* m, size_t s);           // destructor is nullptr
        allocation(void* m, size_t s, std::function<void()> des);
    };

    // Save the meta of allocation
    typedef std::multimap<void*, allocation*> gc_map;

    // gc
    class gc {
    private:
        gc_map map_;
        static size_t level_;                  // The high-level of stop the world
        size_t allocated_size_;                // The size of memory that already allocated

        void mark_alloc(void* p);
        void mark_stack();
        void mark();
        void sweep();
        void run();                             // Run gc to mark and sweep 
    public:
        template <typename T>
        T* gc_new();

        void* gc_malloc(size_t size);

    };

    // TODO variable length template
    template <typename T>
    inline T* gc::gc_new()
    {
        auto ret = malloc(sizeof(T));

        if (!ret) {
            this->run();   
            ret = malloc(sizeof(T));
        }

        if (ret) {
            new (ret)T();
            // Add destructor function
            // FIX ME Maybe throw bad alloc
            auto allocated = new allocation(ret, sizeof(T), [ret] {
                ((T*)ret)->T::~T();
            });

            allocated_size_ += sizeof(T);
        }

        return ret;
    }

    inline void* gc::gc_malloc(size_t size)
    {
        if (this->allocated_size_ > level_) {
            this->run();
        }

        void* ret = malloc(size);

        if (!ret) {
            this->run();
            ret = malloc(size);
        }

        if (ret) {
            auto allocated = new allocation(ret, size);         // FIX ME Maybe throw bad alloc
            this->map_.emplace(ret, allocated);
            allocated_size_ += size;
        }

        return ret;
    }

    inline void gc::mark()
    {
         // mark from stack
        mark_stack();

        // mark from root
        for (auto iter = map_.begin(); iter != map_.end(); ++iter) {
            mark_alloc(iter->second->mem_);
        }
    }

    inline void gc::run() 
    {
        mark();
        sweep();
    }

    inline void gc::sweep()
    {
        for (auto iter = map_.begin(); iter != map_.end();) {
            auto alloc = iter->second;
            if (!(alloc->target_ & GC_MARK)) {
                if (alloc->destructor_)
                    alloc->destructor_();
                free(alloc->mem_);
                map_.erase(iter++);
            } else {
                alloc->target_ = GC_UNMARK;
                ++iter;
            }
        }
    }

    inline void gc::mark_stack()
    {
        // TODO
    }

    inline void gc::mark_alloc(void* p)
    {
        auto iter = map_.find(p);
        if (iter != map_.end() && !(iter->second->target_ & GC_MARK)) {
            auto meta = iter->second;
            iter->second->target_ |= GC_MARK;
    
            for (auto i = 0; i < meta->size_ ; ++i) 
            {
                for (char* p = (char*) meta->mem_ ;
                     p <= (char*)meta->mem_ + meta->size_ - sizeof(char*);
                     ++p)
                {
                    mark_alloc(*(void**)p);
                }
            }
        }
    }


}

#endif