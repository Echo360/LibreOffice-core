/**
 * Description: provide the inteface for updating according to the cycle number
 *
 * @ Author        Create/Modi     Note
 * Xiaofeng Xie    Feb 18, 2004
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * Please acknowledge the author(s) if you use this code in any way.
 */

package net.adaptivebox.global;

public interface IUpdateCycleEngine {
  public void updateCycle(int t);
}