# 编写节点

## 框架说明

* ***Runner***
    
    用于运行节点`NodeAny`继承下的功能模块
  
* ***NodeAny***
    
    用于包装数据处理片段代码的模块，功能模块
  
* ***Message***
    
    声明于`runnable_node.hpp`, 用于节点之间进行消息传递的基础类

* ***属性***

    属性只支持两种数据类型，`double`和`std::string`。其它都通过这两种来转换就好了
  
## 意图

**Runner**作为运行容器，消息中转的支撑类，节点归属于**Runner**，发送给节点的消息实际上是发送到了**Runner**的消息队列，然后**Runner**对收到的消息进行逐个提取派发。节点的消息输出其实是写入到对应节点所在的消息队列。

节点支持放在任意**Runner**里，这样如果某个节点处理消息比较耗时，通过多个**Runner**来平摊时间提高处理速度。

![alt haha](data/concept.png)


## 消息定义

所有的消息继承于`ngraph::Message`, 这个消息其实是对实际数据的一个`shared_ptr`包装

消息需要由创建者保证，消息的数据内容可以由使用者释放，或者说消息是`shared_ptr`，当引用为0的时候，自动释放。

例如，对于`AVFrame`数据，参考`simple_av_message`这个模板进行的封装。

```cpp
typedef simple_av_message<AVPacket, av_packet_clone, av_packet_free> video_packet;
```

## 参考实现

```cpp
class Demuxer_v1 : public node_any_basic<Demuxer_v1> {
protected:
    message_pipe<msgs::video_codecpar> raise_open_;
    message_pipe<msgs::video_packet> raise_packet_;
    message_pipe<msgs::command> queued_command_;
public:
    explicit Demuxer_v1(std::string name = "Demuxer_v1")
        : node_any_basic<Demuxer_v1>(std::move(name)) {

        prop_stream_url_ = add_property<std::string>("stream_url", [this] {
            open_url(prop_stream_url_.value());
        });
        raise_packet_ = register_output_message_<msgs::video_packet>();
        raise_open_ = register_output_message_<msgs::video_codecpar>();

        queued_command_ = register_input_message_handler_(&Demuxer_v1::_process_command, this);
    }
}
```

以上代码里
```cpp
message_pipe<msgs::video_codecpar> raise_open_
```
定义了一个数据管道，然后通过
```cpp
raise_open_ = register_output_message_<msgs::video_codecpar>();
```
对管道进行初始化，通过这样定义了这个节点是如何输出消息的。在后续的使用里，通过
```cpp
bool _on_video_open(AVCodecParameters *codec_parameters_) {
    std::cout << "profile    " << codec_parameters_->profile << "." << codec_parameters_->level << std::endl;
    std::cout << "width      " << codec_parameters_->width << std::endl;
    std::cout << "height     " << codec_parameters_->height << std::endl;

    raise_open_(std::make_shared<msgs::video_codecpar>(codec_parameters_));
    return true;
}
```
`raise_open_`来产生一个流打开的事件消息

`raise_packet_`来产生一个流数据包的事件消息
