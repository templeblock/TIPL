#ifndef REG_HPP
#define REG_HPP
#include "image/reg/linear.hpp"
#include "image/reg/bfnorm.hpp"
namespace image{

namespace reg{

enum reg_cost_type{corr,mutual_info};

template<typename value_type>
struct normalization{
    image::geometry<3> from_geo,to_geo;
    image::vector<3> from_vs,to_vs;
private:
    image::affine_transform<value_type> arg;
    image::transformation_matrix<value_type> T;
    image::transformation_matrix<value_type> iT;
    bool has_T;

private:
    std::shared_ptr<image::reg::bfnorm_mapping<value_type,3> > bnorm_data;
    std::shared_ptr<std::future<void> > reg_thread;
    unsigned char terminated;
    int prog;
    void clear_thread(void)
    {
        if(reg_thread.get())
        {
            terminated = 1;
            reg_thread->wait();
            reg_thread.reset();
        }
    }

public:
    normalization(void):prog(0),has_T(false){}
    ~normalization(void)
    {
        clear_thread();
    }
public:
    void update_affine(void)
    {
        has_T = true;
        T = image::transformation_matrix<value_type>(arg,from_geo,from_vs,to_geo,to_vs);
        iT = T;
        iT.inverse();
    }
    const image::transformation_matrix<value_type>& get_T(void) const{return T;}
    const image::transformation_matrix<value_type>& get_iT(void) const{return iT;}
    const image::affine_transform<value_type> get_arg(void) const{return arg;}
    image::affine_transform<value_type> get_arg(void){return arg;}
    template<typename value_type>
    void set_arg(const value_type& rhs){arg = rhs;update_affine();}
public:
    template<typename image_type,typename vector_type>
    void run_reg(const image_type& from,
                 const vector_type& from_vs_,
                 const image_type& to,
                 const vector_type& to_vs_,
                 int factor,
                 reg_cost_type cost_function,
                 image::reg::reg_type reg_type,
                 int thread_count = std::thread::hardware_concurrency())
    {
        terminated = false;
        has_T = false;
        from_geo = from.geometry();
        to_geo = to.geometry();
        from_vs = from_vs_;
        to_vs = to_vs_;
        bnorm_data.reset(new image::reg::bfnorm_mapping<value_type,3>(to.geometry(),
                                                                  image::geometry<3>(7*factor,9*factor,7*factor)));
        prog = -2;
        if(cost_function == mutual_info)
        {
            image::reg::linear(from,from_vs,to,to_vs,arg,reg_type,image::reg::mutual_information(),terminated);
            image::reg::linear(from,from_vs,to,to_vs,arg,reg_type,image::reg::mutual_information(),terminated);
        }
        else
        {
            image::reg::linear(from,from_vs,to,to_vs,arg,reg_type,image::reg::mt_correlation<image::basic_image<float,3>,
                           image::transformation_matrix<double> >(0),terminated);
            prog = -1;
            image::reg::linear(from,from_vs,to,to_vs,arg,reg_type,image::reg::mt_correlation<image::basic_image<float,3>,
                           image::transformation_matrix<double> >(0),terminated);
        }
        prog = 0;
        update_affine();
        if(terminated)
            return;
        if(!factor || reg_type == image::reg::rigid_body)
        {
            prog = 16;
            return;
        }
        image::basic_image<image_type::value_type,image_type::dimension> new_from(to.geometry());
        image::resample(from,new_from,iT,image::linear);
        image::reg::bfnorm(*bnorm_data.get(),new_from,to,thread_count,terminated,prog);
        prog = 16;
    }
    template<typename image_type,typename vector_type>
    void run_background(const image_type& from,
                 const vector_type& from_vs_,
                 const image_type& to,
                 const vector_type& to_vs_,
                 int factor,
                 reg_cost_type cost_function,
                 image::reg::reg_type reg_type,
                 int thread_count = std::thread::hardware_concurrency())
    {
        clear_thread();
        reg_thread.reset(new std::future<void>(std::async(std::launch::async,
            [this,&from,&from_vs_,&to,&to_vs_,factor,cost_function,reg_type,thread_count]()
        {
            run_reg(from,from_vs_,to,to_vs_,1,cost_function,reg_type,thread_count);
        })));
    }

    int get_prog(void)const{return prog+2;}
    template<typename vtype,typename vtype2>
    void operator()(const vtype& index,vtype2& out)
    {
        if(!has_T)
            update_affine();
        vtype2 pos;
        T(index,pos);// from -> new_from
        if(prog)
        {
            pos += 0.5;
            (*bnorm_data.get())(image::vector<3,int>(pos[0],pos[1],pos[2]),out);
        }
        else
            out = pos;
    }
    template<typename vtype>
    void operator()(vtype& pos)
    {
        vtype out(pos);
        (*this)(out,pos);
    }
};


}
}

#endif//REG_HPP
