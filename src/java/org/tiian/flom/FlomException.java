/*
 * Copyright (c) 2013-2019, Christian Ferrari <tiian@users.sourceforge.net>
 * All rights reserved.
 *
 * This file is part of FLoM, Free Lock Manager
 *
 * FLoM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2.0 as
 * published by the Free Software Foundation.
 *
 * FLoM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FLoM.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.tiian.flom;

/**
 * Exceptions specifically generated by FLoM package
 */
public class FlomException extends Exception {
    static {
        System.loadLibrary("flom_java");
    }

    /**
     * The return code of the native C function that returned an error
     * condition to the JNI wrapper method
     */
    private int ReturnCode;

    /**
     * Build a new FLoM excpetion object
     * @param returnCode is the value returned by the native C function
     */
    public FlomException(int returnCode) {
        super(FlomErrorCodes.getText(returnCode));
        ReturnCode = returnCode;
    }

    /**
     * Get the return code of the native C function that returned an error
     * condition to the JNI wrapper method
     * @return the return code associated to the exception
     */
    public int getReturnCode() { return ReturnCode; }
}
