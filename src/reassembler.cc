#include "reassembler.hh"
#include <vector>
#include <iostream>
#include <tuple>

using namespace std;

Reassembler::Reassembler() : 
  buffer(), 
  cur_need_index(0),
  bytes_in_buffer(0)
  {
  }

uint64_t Reassembler:: bytes_pending() const{
    return bytes_in_buffer;
}

void Reassembler::push_in_buffer(uint64_t first_index, std::string data, bool is_last_substring){
    if(buffer.size() == 0){ // 最开始buffer为空，直接插入
        vector<string> tt_vec ;
        tt_vec.push_back(to_string(first_index));
        tt_vec.push_back(data);
        tt_vec.push_back(is_last_substring ? "1" : "0");
        buffer.emplace_back(tt_vec);
        //cout<<"push in buffer(1) "<< data<< ' '<< first_index <<' '<< is_last_substring<< endl;
        //output_buffer();
        bytes_in_buffer += data.size(); // 修改字节数
        return;
    }
    // 把一个数据放入缓存，有点区间合并的意思
    // 采用合并区间的思想
    uint64_t data_left = first_index; // 区间左端点
    uint64_t data_right = first_index + data.size()-1;  // 区间右端点

    // 找能合并的左右区间
    int i = 0;
    int j = int(buffer.size()-1);

    for(; i<int(buffer.size()); i++){
        if(stoull(buffer[i][0]) + buffer[i][1].size() - 1 >= data_left) break;
    }
    for(; j>=0; j--){
        if(stoull(buffer[j][0]) <= data_right) break;
    }

    // 已经找到了要合并的区间范围，如果i<=j, 进行合并，如果i>j,直接插入
    if(i>j){ //不用合并
        if(i == int(buffer.size())){// 插入在最右侧
            vector<string> tt_vec ;
            tt_vec.push_back(to_string(first_index));
            tt_vec.push_back(data);
            tt_vec.push_back(is_last_substring ? "1" : "0");
            buffer.emplace_back(tt_vec);
            //cout<<"push in buffer(2) "<< data<< ' '<< first_index <<' '<< is_last_substring<< endl;
            //output_buffer();
            bytes_in_buffer += data.size(); // 修改字节数
        } else if(j<0){// 插入在最左侧
            vector<string> tt_vec ;
            tt_vec.push_back(to_string(first_index));
            tt_vec.push_back(data);
            tt_vec.push_back(is_last_substring ? "1" : "0");
            buffer.emplace(buffer.begin(), tt_vec);
            //cout<<"push in buffer(3) "<< data<< ' '<< first_index <<' '<< is_last_substring<< endl;
            //output_buffer();
            bytes_in_buffer += data.size();// 修改字节数
        } else {// 插入在中间，i位置之前
            vector<string> tt_vec ;
            tt_vec.push_back(to_string(first_index));
            tt_vec.push_back(data);
            tt_vec.push_back(is_last_substring ? "1" : "0");
            buffer.emplace(buffer.begin()+i, tt_vec);
            //cout<<"push in buffer(4) "<< data<< ' '<< first_index <<' '<< is_last_substring<< endl;
            //output_buffer();
            bytes_in_buffer += data.size();// 修改字节数
        }
    } else { // 要进行合并
        // 对于左端，比较i和data的左端谁更小，i小，就从i开始，加上i多出来的那部分，data小就不用管
        uint64_t pushing_index ;// min(stoull(buffer[i][0]), first_index)
        string pushing_data;

        if(stoull(buffer[i][0]) < first_index) { // 如果i左边更小
            pushing_index = stoull(buffer[i][0]);
            pushing_data = pushing_data + buffer[i][1].substr(0, first_index - pushing_index);
        } else pushing_index = first_index;
        pushing_data = pushing_data + data; // 加上自己那段
        if(stoull(buffer[j][0]) + buffer[j][1].size() > data_right){// 如果j区间右端点更大
            pushing_data = pushing_data + buffer[j][1].substr( data_right- stoull(buffer[j][0]) + 1 );
        }

        // 已经得到合并的字符串了，现在要把合并的还在buffer里面的删去
        uint64_t dec_nums = j-i+1; // 删去次数
        while(dec_nums--){
            // 一直删i指向的就行
            bytes_in_buffer -= buffer[i][1].size();
            //cout<<"out buffer  " << buffer[i][1]<<endl;
            buffer.erase(buffer.begin()+i);
            //output_buffer();
        }

        // 删完了要插入
        vector<string> tt_vec ;
        tt_vec.push_back(to_string(pushing_index));
        tt_vec.push_back(pushing_data);
        tt_vec.push_back(is_last_substring ? "1" : "0");
        buffer.emplace(buffer.begin()+i, tt_vec);
        //cout<<"push in buffer(5) "<< pushing_data<< ' '<< pushing_index <<' '<< is_last_substring<< endl;
        //output_buffer();
        bytes_in_buffer += pushing_data.size();
    }
}

void Reassembler:: put_in_stream(Writer& output){
    // 只要是在buffer里面的，都是满足要求的，只要按顺序就可以直接放入流的
    while(!buffer.empty() && stoull(buffer[0][0]) == cur_need_index){
        output.push(buffer[0][1]);
        bytes_in_buffer -= buffer[0][1].size();
        cur_need_index += buffer[0][1].size();
        if(buffer[0][2] == "1") output.close();// 最后一个
        // cout<<"put in stream, out buffer : "<< buffer[0][1]<<" cur_need_index:  "<<cur_need_index <<endl;
        buffer.erase(buffer.begin());
        //output_buffer();
    }
}

void Reassembler::insert(uint64_t first_index, std::string data, bool is_last_substring, Writer& output ){
    // 先判断字符串有没有超出范围
    //cout<<"first_index:"<<first_index<<"  "<<"data:"<<data<<"  "<<"is_last_substring:"<<is_last_substring<<"  cur_need_index:"<<cur_need_index<<"  output.available:"<<output.available_capacity()<<endl;
    if(first_index >= cur_need_index + output.available_capacity()){ // 超出了流的容量
        return ;
    } 
    if(first_index + data.size() <= cur_need_index){ // 已经提交过了不需要了
        if(data.empty() && is_last_substring){ // 有意义空串
            //cout<< "push kong in buffer , is_last_subtring: "<<  endl;
            last_empty_flag(first_index);
            put_in_stream(output);
        }
        return ;
    }

    // 部分超出，截断
    if(first_index + data.size() > cur_need_index + output.available_capacity()){  // 后半部分超出
        data = data.substr(0, cur_need_index + output.available_capacity() - first_index);
    }
    if(first_index < cur_need_index){ // 前半部分超出
        data = data.substr(cur_need_index - first_index);
        first_index = cur_need_index;
    }

    // 无意义空串
    if(data.empty() && !is_last_substring){
        return ;
    }
    // 处理完毕，处理的后的data都在有效范围内

    // 有意义空串
    if(data.empty() && is_last_substring){
        last_empty_flag(first_index);
        put_in_stream(output);
    }

    push_in_buffer(first_index, data, is_last_substring); // 放入缓冲
    put_in_stream(output);

}

void Reassembler::last_empty_flag(uint64_t last_pos){
    uint64_t i = 0;
    for(; i<buffer.size(); i++){
        if(last_pos < stoull(buffer[i][0])) break;
    }   

    vector<string> tt_vec ;
    tt_vec.push_back(to_string(last_pos));
    tt_vec.push_back( "" );
    tt_vec.push_back( "1" );
    //cout<< "push kong in buffer , is_last_subtring: "<< last_pos<< endl;
    buffer.emplace(buffer.begin()+i, tt_vec);
}

void Reassembler::output_buffer(){
    cout<<"buffer now : ";
    for(auto i : buffer) 
        cout<<"| "<< i[0] << ' '<<i[1]<<' '<<i[2]<<" |" ;
    cout<<endl;
}