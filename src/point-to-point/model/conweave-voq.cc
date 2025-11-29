/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 NUS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Chahwan Song <songch@comp.nus.edu.sg>
 */

#include "ns3/conweave-voq.h"

#include "ns3/assert.h"
#include "ns3/conweave-routing.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE("ConWeaveVOQ");

namespace ns3 {

ConWeaveVOQ::ConWeaveVOQ() {}
ConWeaveVOQ::~ConWeaveVOQ() {}

std::vector<int> ConWeaveVOQ::m_flushEstErrorhistory; // instantiate static variable  静态成员，用于记录刷新时间估计误差的历史数据，在RescheduleFlush函数中使用


// 初始化conweave VOQ实例，设置数据流键、目的IP地址、刷新时间和额外刷新时间，并安排第一次刷新事件       使用：在创建或更新一个VOQ实例时调用，传入相关的流信息和调度时间
void ConWeaveVOQ::Set(uint64_t flowkey, uint32_t dip, Time timeToFlush, Time extraVOQFlushTime) {
    m_flowkey = flowkey;
    m_dip = dip;
    m_extraVOQFlushTime = extraVOQFlushTime;
    RescheduleFlush(timeToFlush);
}


// 将数据包加入VOQ的FIFO队列末尾                    使用:当交换机收到属于该flowkey的数据包时调用，将数据包添加到对应的VOQ队列中等待后续处理
void ConWeaveVOQ::Enqueue(Ptr<Packet> pkt) { m_FIFO.push(pkt); }   


// 立即清空队列并通过回调函数发送所有数据包          使用:当VOQ需要立即发送所有积压的数据包时调用，例如在流量调度或路径切换时
void ConWeaveVOQ::FlushAllImmediately() {
    m_CallbackByVOQFlush(
        m_flowkey,
        (uint32_t)m_FIFO.size()); /** IMPORTANT: set RxEntry._reordering = false at flushing */
    while (!m_FIFO.empty()) {     // for all VOQ pkts
        Ptr<Packet> pkt = m_FIFO.front();  // get packet
        CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header |
                        CustomHeader::L4_Header);
        pkt->PeekHeader(ch);
        m_switchSendToDevCallback(pkt, ch);  // SlbRouting::DoSwitchSendToDev
        m_FIFO.pop();                        // remove this element
    }
    m_deleteCallback(m_flowkey);  // delete this from SlbRouting::m_voqMap
}


// 通过超时强制刷新VOQ。记录日志，增加统计计数（ConWeaveRouting::m_nFlushVOQTotal），取消下一次的刷新事件，然后立即刷新   使用：不直接由外部调用，而是作为simulator：schedule安排的回调函数
void ConWeaveVOQ::EnforceFlushAll() {
    SLB_LOG(
        "--> *** Finish this epoch by Timeout Enforcement - ConWeaveVOQ Size:" << m_FIFO.size());
    ConWeaveRouting::m_nFlushVOQTotal += 1;  // statistics
    m_checkFlushEvent.Cancel();               // cancel the next schedule
    FlushAllImmediately();                    // flush VOQ immediately
}

/**
 * @brief Reschedule flushing timeout
 * @param timeToFlush relative time to flush from NOW             
 * 使用：当调度算法更新了某个流的刷新时间，或者在set时需要安排初始刷新使用
 */
void ConWeaveVOQ::RescheduleFlush(Time timeToFlush) {
    if (m_checkFlushEvent.IsRunning()) {  // if already exists, reschedule it

        uint64_t prevEst = m_checkFlushEvent.GetTs();
        if (timeToFlush.GetNanoSeconds() == 1) {
            // std::cout << (int(prevEst - Simulator::Now().GetNanoSeconds()) -
            //               m_extraVOQFlushTime.GetNanoSeconds())
            //           << std::endl;
            m_flushEstErrorhistory.push_back(int(prevEst - Simulator::Now().GetNanoSeconds()) -
                                             m_extraVOQFlushTime.GetNanoSeconds());
        }

        m_checkFlushEvent.Cancel();
    }
    m_checkFlushEvent = Simulator::Schedule(timeToFlush, &ConWeaveVOQ::EnforceFlushAll, this);   // 重新安排一个在timeToFlush时间后调用EnforceFlushAll的事件
}

// 检查VOQ是否为空 返回m_FIFO队列是否为空          使用：用于判断VOQ中是否还有待处理的数据包
bool ConWeaveVOQ::CheckEmpty() { return m_FIFO.empty(); }


// 获取VOQ中数据包的数量 返回m_FIFO队列的大小      使用：用于统计和监控VOQ的积压情况
uint32_t ConWeaveVOQ::getQueueSize() { return (uint32_t)m_FIFO.size(); }

}  // namespace ns3