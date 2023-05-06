#include <iostream>
#include <memory>
#include <mutex>
#include <type_traits>

// 懒汉式
//在第一次用到类实例的时候才会去实例化一个对象。在访问量较小，甚至可能不会去访问的情况下，采用懒汉实现，这是以时间换空间。
// 优点：如果不使用shared_ptr，会造成内存泄漏。只负责new对象，没有delete
// 缺点：在某些平台，双检索会失效
class Singleton1{
public:
    typedef std::shared_ptr<Singleton1> ptr;
    Singleton1(Singleton1&) = delete;
    Singleton1& operator=(const Singleton1&) = delete;
    ~Singleton1(){
        std::cout<<"destructor called" << std::endl;
    }
    static ptr get_instance(){
        if(m_instance_ptr == nullptr){
            std::lock_guard<std::mutex> lk(m_mutex);
            if(m_instance_ptr == nullptr){
                m_instance_ptr = std::shared_ptr<Singleton1> (new Singleton1);
            }
        }
        return m_instance_ptr;
    }
private:
    Singleton1(){
        std::cout << "constructor called"  << std::endl;
    }
    static ptr m_instance_ptr;
    static std::mutex m_mutex; 
}; 

// 最推荐的懒汉式单例(magic static) - 局部静态变量
// C++11标准中的magic static特性：
// If control enters the declaration concurrently while the variable is being initialized, the concurrent execution shall wait for completion of the initialization.
// 如果当变量在初始化的时候，并发同时进入声明语句，并发线程将会阻塞等待初始化结束。

class Singleton2{
public:
    Singleton2(const Singleton2&) = delete;
    Singleton2& operator=(const Singleton2&) = delete;
    ~Singleton2(){
        std::cout << "destructor called" << std::endl;
    }

    static Singleton2& get_instance(){
        static Singleton2 instance;
        return instance;
    }
private:
    Singleton2(){
        std::cout << "constructor called" << std::endl;
    }
};

// 系统中可能有多个单例，如果都按以上方式会有代码重复，使用模板或者继承的办法可以实现代码复用
// 使用一个代理类，子类构造函数需要传递token类才能构造，token类定义
template<typename  T>
class Singleton3{
public:
    //std::is_nothrow_constructible<T>::value是一个编译期常量，它的值为true或false。当且仅当T类型的所有构造函数都不会抛出异常时，其值为true；否则，其值为 false。
    static T& get_instance() noexcept(std::is_nothrow_constructible<T>::value){ 
        static T instance{token()};
        return instance;
    }
    virtual ~Singleton3() = default;
    Singleton3(const Singleton3&) = delete;
    Singleton3& operator=(const Singleton3&) = delete;
protected:
    struct token{};
    Singleton3() noexcept = default;
};

class DerivedSingle : public Singleton3<DerivedSingle>{
public:
    DerivedSingle(token){
        std::cout << "constructor called" << std::endl;
    }
    ~DerivedSingle(){
        std::cout << "destructor called" << std::endl;
    }
    DerivedSingle(const DerivedSingle&) = delete;
    DerivedSingle& operator=(const DerivedSingle&) = delete;
};

int main(int argc,char *argv[]){
    DerivedSingle& instance1 = DerivedSingle::get_instance();
    DerivedSingle& instance2 = DerivedSingle::get_instance();
}

//常见的单例模式缺点：
//不同的类想要变成单例需要定义不同的GetInstance()，重复
//不能处理一个类的构造存在重载的情况
//不能处理不同类的构造函数参数个数不同的问题
//无法精确控制单例对象的顺序依赖关系

template<typename T>
class Clobal final{
public:
    static T* Get() {return *GetPPtr();}

    template<typename... Args>
    static void New(Args&&... args){
        assert(Get() == nullptr);
        *GetPPtr() = new T(std::forward<Args>(args)...);
    }

    static void Delete(){
        if(Get() != nullptr){
            delete Get();
            *GetPPtr() = nullptr;
            std::cout << "Delete" << std::endl;
        }
    }

private:
    static T** GetPPtr(){
        static T* ptr = nullptr;
        return &ptr;
    }
};

// 任意一个类都可以通过Global模板成为单例
// 可以用统一的New接口创建各种类单例
// 可以精确地控制单例对象的声明周期和依赖关系
// 把细粒度地解决同步问题的权力交给了开发者

// 饿汉式
// 在单例类定义的时候就进行实例化。在访问量比较大，或者可能访问的线程比较多时，采用饿汉实现，可以实现更好的性能。这是以空间换时间。
// 静态成员变量的初始化/定义是被看做为它自身的类域中
// 私有构造函数的目的并不是禁止对象构造，其目的在于控制哪些代码能够调用这个构造函数，进而限制类对象的创建。
// 私有的构造函数可以被该类的所有成员函数（静态或非静态的）调用，该类的友元类或友元方法也能访问该类的私有函数
// 饿汉模式有一个缺点，m_Singletion是动态初始化（通过执行期间的Singletion构造函数调用），而懒汉模式的m_pSingletion是静态初始化（可通过编译期常量来初始化）。程序的第一条assembly（汇编语言）语句被执行之前，
// 编译器就已经完成了静态初始化（通常静态初始化相关数值或动作（static initializers）就位于“内含可执行程序"的文件中，所以，程序被装载（loading）之际也就是初始化之时）。
// 然而面对不同编译单元（translation unit:大致上你可以把编译单元视为可被编译的C++源码文件）中的动态初始化对象，C++并未定义其间的初始化顺序，这就是麻烦的主要根源，当调用getInstance()时可能返回一个尚未构造的对象）
// 简单来说，潜在问题在于no-local static对象（函数外的static对象）在不同编译单元中的初始化顺序是未定义的，如果在初始化完成之前调用 getInstance() 方法会返回一个未定义的实例。
class Singleton4{
private:
    static Singleton4 m_instance;                        
public:
    static Singleton4& get_instance(){
        return m_instance;
    }
    Singleton4 (const Singleton4&) = delete;
    Singleton4& operator= (const Singleton4&) = delete;
    ~Singleton4(){
        std::cout << "destructor called" << std::endl;
    }
private:
    Singleton4(){
        std::cout << "constructor called" << std::endl;
    }
};
Singleton4 Singleton4::m_instance;                       

// 可以看到这种方式确实非常简洁，同时类仍然具有一般类的特点而不受限制，当然也因此失去了单例那么强的约束（禁止赋值、构造和拷贝构造）
class A{
public:
    A(){
        std::cout << "constructor called" << std::endl;
    }
    ~A(){
        std::cout << "destructor called" << std::endl;
    }
};
template<typename T>
T& get_global(){
    static T instance;
    return instance;
}

