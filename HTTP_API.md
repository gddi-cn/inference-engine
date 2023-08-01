# 概述


当前`gddi::InferenceServer_v0`已实现了如下

## API Reference

### 1. 获取当前所有的任务

>`GET` `http://${host}:${port}/api/task`
    
一次性的返回所有的任务数据

* 返回值
> ```typescript
> interface ITaskNode {
>     ports_in: string[]
>     ports_out: string[]
>     runner: string
>     running: boolean
>     logs: [
>         {
>             "time": string
>             "message": string
>         }
>     ]
> }
> 
> interface ITask {
>     running: boolean
>     name: string
>     nodes: ITaskNode[]
>     quit_code: number     // 如果任务退出了，这个是任务最后的返回数值，不为0表示有异常
>     raw_config: string    // 创建任务使用的配置
>     runners: string[]     // 当前任务使用的线程
> }
> ```

### 2. 创建任务

>`POST` `http://${host}:${port}/api/task`

创建任务

*  `POST`  ***body* 请求参数**
> ```typescript
> interface ITaskCreate {
>     name: string          // 任务名
>     config: string        // 任务配置
> }
> ```

* 返回值
> ```typescript
> interface ITaskCreateResult {
>     error: string
>     success: boolean
>     tasks: ITask[]
> }
> ```

### 3. 删除任务

>`DELETE` `http://${host}:${port}/api/task?task_name={task_name}`

删除指定名称的任务


* 返回值
> ```typescript
> interface ITaskCreateResult {
>     error: string
>     success: boolean
>     tasks: ITask[]
> }
> ```

### 4. 获取当前推理服务支持的节点

>`GET` `http://${host}:${port}/api/node_constraints`

用于前端编辑流程时，验证节点是否合法的配置性信息

注意返回的是一个数组

* 返回值
> ```typescript
> interface INodeConstraint {
>     description: string                   // 简易描述，内嵌于程序内的
>     ep_in: {                              // 输入端点描述
>         feature: string                   // 数据类型
>         index: number                     // 数据端点
>     }[]
>     ep_out:  {                            // 输出端点描述
>         feature: string
>         index: number
>     }[]
>     name: string                          // 节点默认名称
>     type: string                          // 节点类型
>     version: string                       // 节点版本
>     props: {                              // 支持的属性参数字典
>         [index: string]: string | number
>     }
> }[]
> ```

