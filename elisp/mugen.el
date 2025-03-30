;; mugen-mode

(defface mugen-flag-face
  '((t (:foreground "DarkOrange" )))
  "Face for flag strings in mugen-mode."
  :group 'mugen)

(defface mugen-opcode-face
  '((t (:foreground "RoyalBlue" :weight bold)))
  "Face for opcodes in microcode section."
  :group 'mugen)

(defconst mugen-keywords
  '("rom" "address" "signals" "opcodes" "microcode" "cycle" "opcode" "flags"))

(defconst mugen-keywords-regexp
  (concat "\\b" (regexp-opt mugen-keywords t) "\\b"))

(defvar mugen-font-lock-keywords
  (list
   (list mugen-keywords-regexp 1 'font-lock-keyword-face)
   '("\\([A-Za-z0-9_]+\\)\\s-*:\\s-*[0-9xX]+\\s-*:\\s-*[01xX]+" (1 'mugen-opcode-face))
   '("[A-Za-z0-9_]+\\s-*:\\s-*[0-9x]+\\s-*:\\s-*\\([01xX]+\\)" (1 'mugen-flag-face))   
   '("\\b0x[0-9A-Fa-f]+\\b" . 'font-lock-constant-face)
   '("\\b[0-9]+\\b" . 'font-lock-constant-face)))


(defvar mugen-mode-syntax-table
  (let ((st (make-syntax-table)))
    (modify-syntax-entry ?# "<" st)   ; comments start with #
    (modify-syntax-entry ?\n ">" st)  ; newline ends comment
    (modify-syntax-entry ?{ "(}" st)  ; treat { as an open delimiter
    (modify-syntax-entry ?} "){" st)  ; treat } as a closing delimiter
    st))

(defun mugen-indent-line ()
  "Indent current line in Mugen mode."
  (interactive)
  (save-excursion
    (back-to-indentation)
    (cond
     ;; Section headers always align to the left.
     ((looking-at "\\[")
      (indent-line-to 0))

     ;; Closing braces align with the nearest section header.
     ((looking-at "}")
      (let ((depth (nth 0 (syntax-ppss))))
        (indent-line-to (* 2 (max 0 (1- depth))))))

     ;; Normal indentation: 2 spaces per unmatched '{'.
     (t
      (let ((depth (nth 0 (syntax-ppss))))
        (indent-line-to (* 2 depth)))))))

(define-derived-mode mugen-mode prog-mode "Mugen"
  "Major mode for Mugen microcode files."
  :syntax-table mugen-mode-syntax-table
  (setq-local comment-start "# ")
  (setq-local comment-end "")
  (setq-local indent-line-function 'mugen-indent-line)  
  (setq font-lock-defaults '(mugen-font-lock-keywords)))

(add-to-list 'auto-mode-alist '("\\.mu\\'" . mugen-mode))
