#include <vector>
#include <iostream>




//CUDAFY!!
template<typename S, typename T>
void chunk_transform(std::vector<std::vector<S> >& source, std::vector<std::vector<T> >& target, std::vector<float>& weight_vector ){


    static time_t start;
    start = clock();
    static int sample_index = 0;

    //Parallelize this loop with Openmpi or CUDA!

    target.resize(source.size());
    for (int i = 0; i < source.size(); i++) {
        target[i].resize(134);
    }

    //Change it to memcpy version or parallelize
    for( int i = 0; i < source.size(); i++){
        target[i][133] = ++sample_index;

        for(int j = 2; j < 130;j++)
             target[i][j-2] = static_cast<T>(source[i][j])*weight_vector[j-2];

        for(int j = 258; j < 263;j++)
            target[i][j-130] = static_cast<T>(source[i][j])*weight_vector[j-2];


    }





    //std::cout << "copy time "  << 1000.0*static_cast<double>((clock() - start))/CLOCKS_PER_SEC <<  std::endl;
   // std::cout << "chunk transform" << std::endl;

}

